#pragma once

#include <memory>
#include <string>
#include <lexbor/html/html.h>

namespace weave {

class LexborDocument {
public:
    explicit LexborDocument(const std::string& html);
    ~LexborDocument() = default;

    LexborDocument(const LexborDocument&) = delete;
    LexborDocument& operator=(const LexborDocument&) = delete;

    LexborDocument(LexborDocument&&) = default;
    LexborDocument& operator=(LexborDocument&&) = default;

    lxb_html_document_t* get() const { return doc_.get(); }
    lxb_dom_node_t* body() const;
    lxb_dom_node_t* document_element() const;  // Returns <html> element

private:
    struct DocumentDeleter {
        void operator()(lxb_html_document_t* doc) const {
            if (doc) {
                lxb_html_document_destroy(doc);
            }
        }
    };

    std::unique_ptr<lxb_html_document_t, DocumentDeleter> doc_;
};

} // namespace weave