#include "json.hpp"

#include <sstream>
#include <iostream>
#include <fstream>

bool Node::indexable() const { return false; }
bool Node::key_indexable() const { return false; }
bool Node::is_leaf() const { return false; }
std::ostream &Node::dump(std::ostream &os) const { return os; }
Node *Node::clone() const { return new Node(*this); }

ValueNode::ValueNode() : _value() {}
ValueNode::ValueNode(std::string value) : _value(value), _printer([](std::ostream &os, std::string value) -> std::ostream &
                                                                  { return os << "\"" << value << "\""; })
{
}
ValueNode::ValueNode(const char *value) : ValueNode(std::string(value)) {}
ValueNode::ValueNode(bool value) : _value(value ? "1" : "0"), _printer([](std::ostream &os, std::string value) -> std::ostream &
                                                                       { return os << (std::stoi(value) ? "true" : "false"); })
{
}
bool ValueNode::is_leaf() const { return true; };
std::optional<std::string> ValueNode::value() const { return _value; }
std::ostream &ValueNode::dump(std::ostream &os) const
{
    if (value().has_value())
        return _printer(os, value().value());
    return os << "null";
}
ValueNode *ValueNode::clone() const { return new ValueNode(*this); }

bool KeyIndexableNodeI::key_indexable() const { return true; }

bool IndexableNodeI::indexable() const { return true; }

Proxy<Node> ObjectNode::operator[](std::string key) { return children[key]; }
std::ostream &ObjectNode::dump(std::ostream &os) const
{
    os << "{";
    for (auto it = children.begin(); it != children.end(); ++it)
    {
        os << "\"" << it->first << "\": ";
        it->second->dump(os);
        if (std::next(it) != children.end())
            os << ", ";
    }
    return os << "}";
}
ObjectNode *ObjectNode::clone() const
{
    auto clone = new ObjectNode();
    for (auto &[key, child] : children)
    {
        clone->children[key] = child.clone();
    }
    return clone;
}

Proxy<Node> ArrayNode::operator[](size_t idx)
{
    if (idx >= children.size())
    {
        return ValueNode::proxy();
    }
    return children[idx];
}
size_t ArrayNode::size() const { return children.size(); }
void ArrayNode::add_child(Proxy<Node> child) { children.push_back(child); }
std::ostream &ArrayNode::dump(std::ostream &os) const
{
    os << "[";
    for (size_t i = 0; i < children.size(); ++i)
    {
        children[i]->dump(os);
        if (i + 1 < children.size())
            os << ", ";
    }
    return os << "]";
}
ArrayNode *ArrayNode::clone() const
{
    auto clone = new ArrayNode();
    for (auto &child : children)
    {
        clone->children.push_back(child.clone());
    }
    return clone;
}

const char *MalformedJson::what() const noexcept { return "Malformed JSON"; }

Json::Json(std::initializer_list<std::pair<std::string, Json>> list)
{
    Proxy<Node> object = ObjectNode::proxy();
    for (auto &pair : list)
        object.as<ObjectNode>()->operator[](pair.first).reset(pair.second.root);
    root.reset(object);
}
Json Json::clone() const
{
    return Json(root.clone());
}
Json Json::array(std::initializer_list<Json> list)
{
    std::vector<Json> vec(list);
    return Json::array(vec);
}
Json Json::array(std::vector<Json> list)
{
    Proxy<Node> array = ArrayNode::proxy();
    for (auto &elem : list)
        array.as<ArrayNode>()->add_child(elem.root);
    return Json(array);
}
Json Json::operator[](std::string key)
{
    if (root->key_indexable())
        return root.as<KeyIndexableNodeI>()->operator[](key);
    return Json();
}
Json Json::operator[](size_t idx)
{
    if (root->indexable())
        return root.as<IndexableNodeI>()->operator[](idx);
    return Json();
}
Json Json::operator=(Json other)
{
    root.reset(other.root);
    return *this;
}
Json Json::operator=(std::string value)
{
    root.reset(ValueNode::proxy(value));
    return *this;
}
Json Json::operator=(const char *value)
{
    root.reset(ValueNode::proxy(value));
    return *this;
}
Json Json::parse_file(std::string const &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("File not found");
    std::ostringstream oss;
    oss << file.rdbuf();
    return Json::parse_str(oss.str());
}
Json Json::parse_str(std::string const &str)
{
    std::ostringstream oss;

    // Remove whitespace, but not inside strings
    bool inside = false;
    for (char ch : str)
    {
        if (ch == '"')
            inside = !inside;
        if (std::isspace(ch) && !inside)
            continue;
        oss << ch;
    }

    std::string str_clean = oss.str();

    size_t index = 0;
    return parse_recursively(str_clean, index);
}
Json Json::parse_recursively(std::string &json, size_t &index)
{
    switch (json[index])
    {
    case '{':
        return parse_object(json, index);
    case '[':
        return parse_array(json, index);
    default:
        return parse_value(json, index);
    }
}
Json Json::parse_object(std::string &json, size_t &index)
{
    ++index; // Skip opening curly brace
    Json object;
    while (json[index] != '}')
    {
        std::ostringstream key;
        if (json[index++] != '"')
            throw MalformedJson();
        while (json[index] != '"')
        {
            key << json[index++];
        }
        ++index; // Skip final quote
        ++index; // Skip colon
        Json value = parse_recursively(json, index);
        if (json[index] != '}')
            ++index; // Skip comma
        object[key.str()] = value;
    }
    ++index; // Skip closing curly brace
    return object;
}
Json Json::parse_array(std::string &json, size_t &index)
{
    ++index; // Skip opening square bracket
    Proxy<Node> array = ArrayNode::proxy();
    while (json[index] != ']')
    {
        array.as<ArrayNode>()->add_child(parse_recursively(json, index).root);
        if (json[index] != ']')
            ++index; // Skip comma
    }
    ++index; // Skip closing square bracket
    return Json(array);
}
Json Json::parse_value(std::string &json, size_t &index)
{
    if (json.substr(index, 4) == "true")
    {
        index += 4;
        return Json(true);
    }
    if (json.substr(index, 5) == "false")
    {
        index += 5;
        return Json(false);
    }
    auto is_number = [](char ch)
    { return '0' <= ch && ch <= '9'; };
    if (is_number(json[index]))
    {
        std::ostringstream number;
        bool is_float = false;
        while (json[index] == '.' || is_number(json[index]))
        {
            if (json[index] == '.')
                is_float = true;
            number << json[index++];
        }
        if (is_float)
            return Json(std::stof(number.str()));
        return Json(std::stol(number.str()));
    }
    if (json[index++] == '"')
    {
        std::ostringstream string;
        while (json[index] != '"')
        {
            string << json[index++];
        }
        ++index; // Skip last quote
        return Json(string.str());
    }
    throw MalformedJson();
}
