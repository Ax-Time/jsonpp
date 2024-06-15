#ifndef JSON_HPP
#define JSON_HPP

#include "proxy.hpp"

#include <optional>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <exception>
#include <iostream>
#include <concepts>
#include <initializer_list>
#include <functional>

template <typename T>
concept ConvertibleToStdString = requires(T a) { std::to_string(a); };

class Node
{
public:
    virtual ~Node() = default;
    virtual bool indexable() const;
    virtual bool key_indexable() const;
    virtual bool is_leaf() const;
    virtual std::ostream &dump(std::ostream &os) const;
    friend std::ostream &operator<<(std::ostream &os, const Node &node)
    {
        return node.dump(os);
    }
};

class ValueNode : public Node
{
private:
    std::function<std::ostream &(std::ostream &, std::string)> _printer;
    std::optional<std::string> _value;
    ValueNode();
    ValueNode(std::string value);
    ValueNode(const char *value);
    template <ConvertibleToStdString T>
    ValueNode(T value) : _value(std::to_string(value)), _printer(
                                                            [](std::ostream &os, std::string value) -> std::ostream &
                                                            { return os << value; })
    {
    }
    ValueNode(bool value);

public:
    template <typename... Args>
    static Proxy<Node> proxy(Args... args)
    {
        auto ptr = std::shared_ptr<ValueNode>(new ValueNode(args...));
        return Proxy<Node>(ptr);
    }
    bool is_leaf() const override;
    std::optional<std::string> value() const;
    std::ostream &dump(std::ostream &os) const override;
};

class KeyIndexableNodeI : public Node
{
public:
    virtual Proxy<Node> operator[](std::string key) = 0;
    bool key_indexable() const override;
};

class IndexableNodeI : public Node
{
public:
    virtual Proxy<Node> operator[](size_t idx) = 0;
    bool indexable() const override;
    virtual size_t size() const = 0;
};

class ObjectNode : public KeyIndexableNodeI
{
private:
    std::map<std::string, Proxy<Node>> children;
    ObjectNode() = default;

public:
    template <typename... Args>
    static Proxy<Node> proxy(Args... args)
    {
        auto ptr = std::shared_ptr<ObjectNode>(new ObjectNode(args...));
        return Proxy<Node>(ptr);
    }
    Proxy<Node> operator[](std::string key) override;
    std::ostream &dump(std::ostream &os) const override;
};

class ArrayNode : public IndexableNodeI
{
private:
    std::vector<Proxy<Node>> children;
    ArrayNode() = default;

public:
    template <typename... Args>
    static Proxy<Node> proxy(Args... args)
    {
        auto ptr = std::shared_ptr<ArrayNode>(new ArrayNode(args...));
        return Proxy<Node>(ptr);
    }
    Proxy<Node> operator[](size_t idx) override;
    size_t size() const override;
    void add_child(Proxy<Node> child);
    std::ostream &dump(std::ostream &os) const override;
};

class Json
{
private:
    Proxy<Node> root;
    static Json parse_recursively(std::string &json, size_t &index);
    static Json parse_object(std::string &json, size_t &index);
    static Json parse_array(std::string &json, size_t &index);
    static Json parse_value(std::string &json, size_t &index);

public:
    Json() : root(ObjectNode::proxy()) {}
    Json(const Json &other) : root(other.root) {}
    Json(Proxy<Node> root) : root(root) {}
    template <ConvertibleToStdString T>
    Json(T value) : root(ValueNode::proxy(value)) {}
    template <ConvertibleToStdString T>
    Json(std::vector<T> value)
    {
        auto array = ArrayNode::proxy();
        for (T &item : value)
        {
            array.as<ArrayNode>()->add_child(ValueNode::proxy(item));
        }
        root.reset(array);
    }
    Json(std::string value) : root(ValueNode::proxy(value)) {}
    Json(const char *value) : root(ValueNode::proxy(value)) {}
    Json(bool value) : root(ValueNode::proxy(value)) {}
    static Json array(std::initializer_list<Json> list);
    static Json array(std::vector<Json> list);
    Json operator[](std::string key);
    Json operator=(Json other);
    Json operator=(std::string value);
    Json operator=(const char *value);
    template <ConvertibleToStdString T>
    Json operator=(std::vector<T> value)
    {
        auto array = ArrayNode::proxy();
        for (T &item : value)
        {
            array.as<ArrayNode>()->add_child(ValueNode::proxy(item));
        }
        root.reset(array);
        return *this;
    }
    template <ConvertibleToStdString T>
    Json operator=(T value)
    {
        root.reset(ValueNode::proxy(value));
        return *this;
    }
    template <typename T>
    std::optional<T> to() const;
    template <>
    std::optional<long> to<long>() const
    {
        std::optional<int> result;
        if (root->is_leaf())
        {
            std::optional<std::string> s = root.as<ValueNode>()->value();
            if (s.has_value())
            {
                try
                {
                    result = std::stol(s.value());
                }
                catch (std::invalid_argument &e)
                {
                }
            }
        }
        return result;
    }
    template <>
    std::optional<double> to<double>() const
    {
        std::optional<double> result;
        if (root->is_leaf())
        {
            std::optional<std::string> s = root.as<ValueNode>()->value();
            if (s.has_value())
            {
                try
                {
                    result = std::stod(s.value());
                }
                catch (std::invalid_argument &e)
                {
                }
            }
        }
        return result;
    }
    template <>
    std::optional<int> to<int>() const { return to<long>(); }
    template <>
    std::optional<short> to<short>() const { return to<long>(); }
    template <>
    std::optional<float> to<float>() const { return to<double>(); }
    template <>
    std::optional<std::string> to<std::string>() const
    {
        std::optional<std::string> result;
        if (root->is_leaf())
        {
            result = root.as<ValueNode>()->value();
        }
        return result;
    }
    template <typename T>
    std::optional<std::vector<T>> asVector() const
    {
        std::optional<std::vector<T>> result;
        if (root->indexable())
        {
            result = std::vector<T>();
            for (size_t i = 0; i < root.as<IndexableNodeI>()->size(); i++)
            {
                std::optional<T> value = Json(root.as<IndexableNodeI>()->operator[](i)).to<T>();
                if (value.has_value())
                {
                    result.value().push_back(value.value());
                }
            }
        }
        return result;
    }
    static Json parse_str(std::string const &json);
    static Json parse_file(std::string const &filename);
    friend std::ostream &operator<<(std::ostream &os, const Json &json)
    {
        os << json.root;
        return os;
    }
};

// Parsing
class MalformedJson : public std::exception
{
public:
    const char *what() const noexcept override;
};

#endif
