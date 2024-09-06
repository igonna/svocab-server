#include "html_extractor.h"
#include <gumbo.h>

namespace core::text_extraction
{
std::tuple<std::string, std::string> extract_text_and_title_from_html(const std::string& html) {
    // Parse the HTML
    GumboOutput* output = gumbo_parse(html.c_str());

    std::string title;
    std::string body_text;

    // Function to recursively extract text from GumboNode
    static auto extract_text = [](GumboNode* node, auto& extract_text_ref, std::string& text_str) -> void {
        if (node->type == GUMBO_NODE_TEXT) {
            text_str += node->v.text.text;
            text_str += " ";
        } else if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag != GUMBO_TAG_SCRIPT && node->v.element.tag != GUMBO_TAG_STYLE) {
            GumboVector* children = &node->v.element.children;
            for (uint32_t i = 0; i < children->length; ++i) {
                extract_text_ref(static_cast<GumboNode*>(children->data[i]), extract_text_ref, text_str);
            }
        }
    };

    // Find the <body> tag and extract its text
    GumboNode* root = output->root;
    if (root->type == GUMBO_NODE_ELEMENT && root->v.element.tag == GUMBO_TAG_HTML) {
        GumboVector* rootChildren = &root->v.element.children;
        for (uint32_t i = 0; i < rootChildren->length; ++i) {
            GumboNode* child = static_cast<GumboNode*>(rootChildren->data[i]);

            if (child->type == GUMBO_NODE_ELEMENT && child->v.element.tag == GUMBO_TAG_HEAD) {
                // Search for <title> in the <head> section
                GumboVector* head_children = &child->v.element.children;
                for (uint32_t j = 0; j < head_children->length; ++j) {
                    GumboNode* head_child = static_cast<GumboNode*>(head_children->data[j]);
                    if (head_child->type == GUMBO_NODE_ELEMENT && head_child->v.element.tag == GUMBO_TAG_TITLE) {
                        if (head_child->v.element.children.length > 0) {
                            GumboNode* title_text_node = static_cast<GumboNode*>(head_child->v.element.children.data[0]);
                            if (title_text_node->type == GUMBO_NODE_TEXT) {
                                title = title_text_node->v.text.text;
                            }
                        }
                        break;  // Stop after finding <title>
                    }
                }
            }

            if (child->type == GUMBO_NODE_ELEMENT && child->v.element.tag == GUMBO_TAG_BODY) {
                extract_text(child, extract_text, body_text);
                break;  // Stop after finding and processing <body>
            }
        }
    }

    // Clean up Gumbo-Parser output
    gumbo_destroy_output(&kGumboDefaultOptions, output);

    return std::tuple<std::string, std::string>(title, body_text);
}
}