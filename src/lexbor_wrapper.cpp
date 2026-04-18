#include "lexbor_wrapper.hpp"
#include <stdexcept>
#include <cstring>

namespace weave {

LexborDocument::LexborDocument(const std::string& html) {
    lxb_html_document_t* doc = lxb_html_document_create();
    if (!doc) {
        throw std::runtime_error("Failed to create Lexbor document");
    }

    lxb_status_t status = lxb_html_document_parse(doc, 
        reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
    
    if (status != LXB_STATUS_OK) {
        lxb_html_document_destroy(doc);
        throw std::runtime_error("Failed to parse HTML document");
    }

    doc_.reset(doc);
}

lxb_dom_node_t* LexborDocument::body() const {
    if (!doc_) {
        return nullptr;
    }

    lxb_dom_element_t* html_element = lxb_dom_document_element(lxb_dom_interface_document(doc_.get()));
    if (!html_element) {
        return nullptr;
    }

    // Look for body element
    lxb_dom_node_t* child = lxb_dom_node_first_child(lxb_dom_interface_node(html_element));
    while (child) {
        if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            lxb_dom_element_t* element = lxb_dom_interface_element(child);
            size_t tag_name_length;
            const lxb_char_t* tag_name = lxb_dom_element_local_name(element, &tag_name_length);
            if (tag_name && tag_name_length == 4 && 
                memcmp(tag_name, "body", 4) == 0) {
                return child;
            }
        }
        child = lxb_dom_node_next(child);
    }

    // If no body found, return html element
    return lxb_dom_interface_node(html_element);
}

lxb_dom_node_t* LexborDocument::document_element() const {
    if (!doc_) {
        return nullptr;
    }

    lxb_dom_element_t* html_element = lxb_dom_document_element(lxb_dom_interface_document(doc_.get()));
    if (!html_element) {
        return nullptr;
    }

    return lxb_dom_interface_node(html_element);
}

} // namespace weave