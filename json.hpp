#ifndef AX_JSON_SINGLE_INCLUDE_HPP
#define AX_JSON_SINGLE_INCLUDE_HPP

#include <memory>
#include <type_traits>
#include <optional>
#include <string>
#include <map>
#include <vector>
#include <exception>
#include <iostream>
#include <concepts>
#include <initializer_list>
#include <functional>
#include <sstream>
#include <fstream>

namespace ax
{
    class MalformedJson : public std::exception
    {
    public:
        const char *what() const noexcept override { return "Malformed JSON"; }
    };

    template <typename T>
    class Proxy
    {
        /**
         * The Proxy class is a wrapper around an object T.
         * It is useful to keep a shared reference to the same object of class T across multiple instances of Proxy.
         * It provides a transparent interface to the object T.
         */
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
        static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");
        // Must have clone method
        static_assert(std::is_same_v<decltype(std::declval<T>().clone()), T *>, "T must have clone method");

    private:
        std::shared_ptr<std::shared_ptr<T>> pp;

    public:
        /**
         * The Proxy class constructor. It passes the arguments to the constructor of T.
         */
        template <typename... Args>
        Proxy(Args... args) : pp(std::make_shared<std::shared_ptr<T>>(std::make_shared<T>(args...))) {}
        /**
         * It creates a new Proxy from a shared pointer to an object of class U, which must be a subclass of T.
         */
        template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>, typename... Args>
        Proxy(std::shared_ptr<U> up) : pp(std::make_shared<std::shared_ptr<T>>(static_cast<std::shared_ptr<T>>(up))) {}
        /**
         * The Proxy class copy constructor. It creates a new Proxy instance that shares the same object of class T.
         */
        Proxy(const Proxy<T> &other) : pp(other.pp) {}
        /**
         * It constructs a new object of class T and makes all copies of this proxy object reference the new object.
         */
        template <typename... Args>
        void reset(Args... args) { *pp = std::make_shared<T>(args...); }
        /**
         * It makes all copies of this proxy object reference the same object as the other proxy object.
         */
        void reset(const Proxy<T> &other) { *pp = *other.pp; }
        /**
         * It returns a reference to the underlying object of class T.
         */
        T &operator*() const { return **pp; }
        /**
         * It returns a pointer to the underlying object of class T.
         */
        T *operator->() const { return (*pp).get(); }
        /**
         * It returns a pointer to the underlying object of class T, casted to class U.
         */
        template <typename U>
        U *as() const { return reinterpret_cast<U *>((*pp).get()); }
        /**
         * It returns a new Proxy instance that references a copy of the underlying object of class T.
         */
        Proxy<T> clone() const
        {
            T *clone = (*pp)->clone();
            return Proxy<T>(std::shared_ptr<T>(clone));
        }
        friend std::ostream &operator<<(std::ostream &os, const Proxy<T> &proxy)
        {
            os << **proxy.pp;
            return os;
        }
    };

    template <typename T>
    concept ConvertibleToStdString = requires(T a) { std::to_string(a); };

    class Node
    {
    public:
        virtual ~Node() = default;
        virtual bool indexable() const { return false; }
        virtual bool key_indexable() const { return false; }
        virtual bool is_leaf() const { return false; }
        virtual std::ostream &dump(std::ostream &os) const { return os; }
        virtual Node *clone() const { return new Node(*this); }
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
        ValueNode() = default;
        ValueNode(ValueNode const &) = default;
        ValueNode(std::string value) : _value(value), _printer([](std::ostream &os, std::string value) -> std::ostream &
                                                               { return os << "\"" << value << "\""; })
        {
        }
        ValueNode(const char *value) : ValueNode(std::string(value)) {}
        template <ConvertibleToStdString T>
        ValueNode(T value) : _value(std::to_string(value)), _printer(
                                                                [](std::ostream &os, std::string value) -> std::ostream &
                                                                { return os << value; })
        {
        }
        ValueNode(bool value) : _value(value ? "1" : "0"), _printer([](std::ostream &os, std::string value) -> std::ostream &
                                                                    { return os << (std::stoi(value) ? "true" : "false"); })
        {
        }

    public:
        template <typename... Args>
        static Proxy<Node> proxy(Args... args)
        {
            auto ptr = std::shared_ptr<ValueNode>(new ValueNode(args...));
            return Proxy<Node>(ptr);
        }
        bool is_leaf() const override { return true; };
        std::optional<std::string> value() const { return _value; }
        std::ostream &dump(std::ostream &os) const override
        {
            if (value().has_value())
                return _printer(os, value().value());
            return os << "null";
        }
        virtual ValueNode *clone() const override { return new ValueNode(*this); }
    };

    class KeyIndexableNodeI : public Node
    {
    public:
        virtual Proxy<Node> operator[](std::string key) = 0;
        bool key_indexable() const override { return true; }
    };

    class IndexableNodeI : public Node
    {
    public:
        virtual Proxy<Node> operator[](size_t idx) = 0;
        bool indexable() const override { return true; }
        virtual size_t size() const = 0;
    };

    class ObjectNode : public KeyIndexableNodeI
    {
    private:
        std::map<std::string, Proxy<Node>> children;
        ObjectNode() = default;
        ObjectNode(ObjectNode const &) = default;

    public:
        template <typename... Args>
        static Proxy<Node> proxy(Args... args)
        {
            auto ptr = std::shared_ptr<ObjectNode>(new ObjectNode(args...));
            return Proxy<Node>(ptr);
        }
        Proxy<Node> operator[](std::string key) override { return children[key]; }
        std::ostream &dump(std::ostream &os) const override
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
        virtual ObjectNode *clone() const override
        {
            auto clone = new ObjectNode();
            for (auto &[key, child] : children)
            {
                clone->children[key] = child.clone();
            }
            return clone;
        }
    };

    class ArrayNode : public IndexableNodeI
    {
    private:
        std::vector<Proxy<Node>> children;
        ArrayNode() = default;
        ArrayNode(ArrayNode const &) = default;

    public:
        template <typename... Args>
        static Proxy<Node> proxy(Args... args)
        {
            auto ptr = std::shared_ptr<ArrayNode>(new ArrayNode(args...));
            return Proxy<Node>(ptr);
        }
        Proxy<Node> operator[](size_t idx) override
        {
            if (idx >= children.size())
            {
                return ValueNode::proxy();
            }
            return children[idx];
        }
        size_t size() const override { return children.size(); }
        void add_child(Proxy<Node> child) { children.push_back(child); }
        std::ostream &dump(std::ostream &os) const override
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
        virtual ArrayNode *clone() const override
        {
            auto clone = new ArrayNode();
            for (auto &child : children)
            {
                clone->children.push_back(child.clone());
            }
            return clone;
        }
    };

    class Json
    {
    private:
        Proxy<Node> root;
        static Json parse_recursively(std::string &json, size_t &index)
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
        static Json parse_object(std::string &json, size_t &index)
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
        static Json parse_array(std::string &json, size_t &index)
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
        static Json parse_value(std::string &json, size_t &index)
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
        Json(std::initializer_list<std::pair<std::string, Json>> list)
        {
            Proxy<Node> object = ObjectNode::proxy();
            for (auto &pair : list)
                object.as<ObjectNode>()->operator[](pair.first).reset(pair.second.root);
            root.reset(object);
        }
        Json(std::string value) : root(ValueNode::proxy(value)) {}
        Json(const char *value) : root(ValueNode::proxy(value)) {}
        Json(bool value) : root(ValueNode::proxy(value)) {}
        Json clone() const
        {
            return Json(root.clone());
        }
        static Json array(std::initializer_list<Json> list)
        {
            std::vector<Json> vec(list);
            return Json::array(vec);
        }
        static Json array(std::vector<Json> list)
        {
            Proxy<Node> array = ArrayNode::proxy();
            for (auto &elem : list)
                array.as<ArrayNode>()->add_child(elem.root);
            return Json(array);
        }
        Json operator[](std::string key)
        {
            if (root->key_indexable())
                return root.as<KeyIndexableNodeI>()->operator[](key);
            return Json();
        }
        Json operator[](size_t idx)
        {
            if (root->indexable())
                return root.as<IndexableNodeI>()->operator[](idx);
            return Json();
        }
        Json operator=(Json other)
        {
            root.reset(other.root);
            return *this;
        }
        Json operator=(std::string value)
        {
            root.reset(ValueNode::proxy(value));
            return *this;
        }
        Json operator=(const char *value)
        {
            root.reset(ValueNode::proxy(value));
            return *this;
        }
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
        static Json parse_str(std::string const &str)
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
        static Json parse_file(std::string const &filename)
        {
            std::ifstream file(filename);
            if (!file.is_open())
                throw std::runtime_error("File not found");
            std::ostringstream oss;
            oss << file.rdbuf();
            return Json::parse_str(oss.str());
        }
        friend std::ostream &operator<<(std::ostream &os, const Json &json)
        {
            os << json.root;
            return os;
        }
    };
}

#endif // AX_JSON_SINGLE_INCLUDE_HPP