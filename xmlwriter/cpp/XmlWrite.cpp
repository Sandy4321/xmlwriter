#include <assert.h>

#include "XmlWrite.h"
#include "base64.h"

bool RAISE_ON_ERROR = true;

std::string encodeString(const std::string &theS,
                         const std::string &theCharPrefix) {
    if (theCharPrefix.size() != 1) {
        std::ostringstream err;
        err << "Prefix for encoding string must be a single character, not \"";
        err <<theCharPrefix << "\"";
        throw ExceptionXml(err.str());
    }
    // Threee steps:
    // * Base64 encoding of theS
    // * Character substitution of the base64 sting.
    // * Prepend prefix.
    std::string base64;
    base64_encode(theS, base64);
    for (size_t i = 0; i < base64.size(); ++i) {
        switch (base64[i]) {
            case '+':
                base64[i] = '-';
                break;
            case '/':
                base64[i] = '.';
                break;
            case '=':
                base64[i] = '_';
                break;
            default:
                break;
        }
    }
    return theCharPrefix + base64;
}

std::string decodeString(const std::string &theS) {
    std::string result;
    for (size_t i = 1; i < theS.size(); ++i) {
        if (theS[i] == '-') {
            result.push_back('+');
        } else if (theS[i] == '.') {
            result.push_back('/');
        } else if (theS[i] == '_') {
            result.push_back('=');
        } else {
            result.push_back(theS[i]);
        }
    }
    result = base64_decode(result);
//    std::cout << "Decode was: \"" << theS << "\" now \"" << result << "\"" << std::endl;
    // This does not work, see the pbXmlWrite.cpp decodeString for the solution.
    // return py::bytes(result);
    return result;
}

std::string nameFromString(const std::string &theStr) {
    return encodeString(theStr, "Z");
}

XmlStream::XmlStream(const std::string &theEnc/* ='utf-8'*/,
              const std::string &theDtdLocal /* =None */,
              int theId /* =0 */,
              bool mustIndent /* =True */) : encodeing(theEnc),
                                             dtdLocal(theDtdLocal),
                                             _mustIndent(mustIndent),
                                             _intId(theId),
                                             _inElem(false) {}

std::string XmlStream::getvalue() const {
    return m_output.str();
}

std::string XmlStream::id() {
    std::ostringstream out;
    ++_intId;
    return out.str();
}

bool XmlStream::_canIndent() const {
    for (auto value: _canIndentStk) {
        if (! value) {
            return false;
        }
    }
    return true;
}

void XmlStream::_flipIndent(bool theBool) {
    assert(_canIndentStk.size() > 0);
    _canIndentStk[_canIndentStk.size() - 1] = theBool;
}

void XmlStream::xmlSpacePreserve() {
    _flipIndent(false);
}

void XmlStream::startElement(const std::string &name, const tAttrs &attrs) {
//    std::cout << "XmlStream::startElement: " << name << std::endl;
    _closeElemIfOpen();
//    std::cout << "Help XmlStream::startElement: _indent()" << std::endl;
    _indent();
//    std::cout << "Help XmlStream::startElement: m_output" << std::endl;
    m_output << '<' << name;
    std::string attribute_value;
    bool use_attribute_value;
    for (auto iter: attrs) {
        m_output << ' ' << iter.first << '=' << "\"";
        use_attribute_value = _encode(iter.second, attribute_value);
        if (use_attribute_value) {
            m_output << attribute_value;
        } else {
            m_output << iter.second;
        }
        m_output << "\"";
    }
    _inElem = true;
    _canIndentStk.push_back(_mustIndent);
    _elemStk.push_back(name);
}

void XmlStream::characters(const std::string &theString) {
    _closeElemIfOpen();
    std::string encoded;
    if (_encode(theString, encoded)) {
        m_output << encoded;
    } else {
        m_output << theString;
    }
    // mixed content - don't indent
    _flipIndent(false);
}

void XmlStream::literal(const std::string &theString) {
    _closeElemIfOpen();
    m_output << theString;
    // mixed content - don't indent
    _flipIndent(false);
}

void XmlStream::comment(const std::string &theS, bool newLine) {
    _closeElemIfOpen();
    if (newLine) {
        _indent();
    }
    std::string encoded;
    if (_encode(theS, encoded)) {
        m_output << "<!--" << encoded << "-->";
    } else {
        m_output << "<!--" << theS << "-->";
    }
}

void XmlStream::pI(const std::string &theS) {
    _closeElemIfOpen();
    std::string encoded;
    if (_encode(theS, encoded)) {
        m_output << "<?" << encoded << "?>";
    } else {
        m_output << "<?" << theS << "?>";
    }
    // mixed content - don't indent
    _flipIndent(false);
}

void XmlStream::endElement(const std::string &name) {
//    std::cout << "XmlStream::endElement: " << name << std::endl;
    if (_elemStk.size() == 0) {
        throw ExceptionXmlEndElement("endElement() on empty stack");
    }
    if (name != _elemStk[_elemStk.size() - 1]) {
        std::ostringstream err;
        err << "endElement(\"" << name << "\") does not match \"";
        err << _elemStk[_elemStk.size() - 1] << "\"";
        throw ExceptionXmlEndElement(err.str());
    }
    _elemStk.pop_back();
    if (_inElem) {
        m_output << " />";
        _inElem = false;
    } else {
        _indent();
        m_output << "</" << name << '>';
    }
    _canIndentStk.pop_back();
}

void XmlStream::writeECMAScript(const std::string &theScript) {
    startElement("script",
                 {
                     std::pair<std::string, std::string>(
                                                         "type",
                                                         "text/ecmascript"
                                                         )
                 });
    writeCDATA(theScript);
    endElement("script");
}

void XmlStream::writeCDATA(const std::string &theData) {
    _closeElemIfOpen();
    xmlSpacePreserve();
//    m_output << '';
    m_output << "\n<![CDATA[\n";
    m_output << theData;
    m_output << "\n]]>\n";
}

void XmlStream::writeCSS(const std::map<std::string, tAttrs> &theCSSMap) {

    startElement("style",
                 {
                     std::pair<std::string, std::string>("type", "text/css")
                 });
    for(auto style_map: theCSSMap) {
        m_output << style_map.first << " {\n";
        for (auto attr_value: style_map.second) {
            m_output << attr_value.first << " : " << attr_value.second << ";\n";
        }
        m_output << "}\n";
    }
    endElement("style");
}

void XmlStream::_indent(size_t offset) {
    if (_canIndent()) {
        m_output << '\n';
        while(offset < _elemStk.size()) {
            m_output << INDENT_STR;
            ++offset;
        }
    }
}

void XmlStream::_closeElemIfOpen() {
    if (_inElem) {
        m_output << '>';
        _inElem = false;
    }
}

void XmlStream::_write_to_output(const std::string &input,
                                 std::string &output,
                                 const std::string &subst,
                                 size_t &index_start,
                                 size_t index_current
                                 ) const {
    if (output.size() == 0) {
        output.reserve(input.size() * 2);
    }
    output.append(input, index_start, index_current - index_start);
    output.append(subst);
    index_start = index_current + 1;
}

// Encode the input to the output
// Returns true if output must be used else the input can be used directly.
bool XmlStream::_encode(const std::string &input,
                        std::string &output) const {
    output.clear();
    bool use_original = true;
    size_t index_start = 0;
    size_t index_current = 0;
    
    for (auto chr: input) {
        switch (chr) {
            case '<':
                _write_to_output(input, output, "&lt;",
                                 index_start, index_current);
                use_original = false;
                break;
            case '>':
                _write_to_output(input, output, "&gt;",
                                 index_start, index_current);
                use_original = false;
                break;
            case '&':
                _write_to_output(input, output, "&amp;",
                                 index_start, index_current);
                use_original = false;
                break;
            case '\'':
                _write_to_output(input, output, "&apos;",
                                 index_start, index_current);
                use_original = false;
                break;
            case '"':
                _write_to_output(input, output, "&quot;",
                                 index_start, index_current);
                use_original = false;
                break;
            default:
                if (! use_original) {
                    output.push_back(chr);
                    ++index_start;
                }
                break;
        }
        ++index_current;
    }
    return ! use_original;
}

XmlStream &XmlStream::_enter() {
    m_output << "<?xml version='1.0' encoding=\"" << encodeing << "\"?>";
    return *this;
}

//bool XmlStream::_exit(py::args args) {
//    _close();
//    return false; // Propogate any exception
//}

void XmlStream::_close() {
    while (_elemStk.size()) {
        endElement(_elemStk[_elemStk.size() - 1]);
    }
    m_output << '\n';
}

/*************** XhtmlStream **************/
XhtmlStream::XhtmlStream(const std::string &theEnc/* ='utf-8'*/,
                         const std::string &theDtdLocal /* =None */,
                         int theId /* =0 */,
                         bool mustIndent /* =True */) : XmlStream(theEnc,
                                                                  theDtdLocal,
                                                                  theId,
                                                                  mustIndent)
{
}

XhtmlStream &XhtmlStream::_enter() {
    XmlStream::_enter();
    m_output << "\n<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"";
    m_output << " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">";
    startElement("html", ROOT_ATTRIBUTES);
    return *this;
}

// Writes the string replacing any ``\\n`` characters with ``<br/>`` elements.
void XhtmlStream::charactersWithBr(const std::string &sIn) {
    size_t index = 0;
    while (index < sIn.size()) {
        size_t found = sIn.find("\n", index);
        if (found != std::string::npos) {
            std::string slice(sIn, index, found - index);
            characters(slice);
            startElement("br", tAttrs());
            endElement("br");
            index = found + 1;
        } else {
            if (index < sIn.size()) {
                std::string slice(sIn, index);
                characters(slice);
                break;
            }
        }
    }
}

/*************** XhtmlStream **************/
