# jsonpp
A simple c++ library to handle json files.

## Compiling
The library requires c++20 to compile.

## Usage

### Create a json object
```cpp
#include "json.hpp"

int main()
{
    ax::Json my_json = {
        {"name", "John"},
        {"age", 25},
        {"is_student", true},
        {"grades", ax::Json::array({10, 9, "A", "B", 6})}
    };
    std::cout << my_json << std::endl; // -> {"name":"John","age":25,"is_student":true,"grades":[10,9,"A","B",6]}
    return 0;
}
```
### Modify an object
```cpp
#include "json.hpp"

int main()
{
    ax::Json my_json = {
        {"age", 4}, 
        {"school", "Kindergarten"}
    };
    my_json["age"] = 6;
    my_json["school"] = "Elementary";
    std::cout << my_json << std::endl; // -> {"age":6,"school":"Elementary"}
    return 0;
}
```
```cpp
#include "json.hpp"

int main()
{
    ax::Json my_json{{"name", "Axel"}};
    ax::Json metadata{
        {"age", 23},
        {"height", 1.75},
        {"is_student", true}
    };
    my_json["metadata"] = metadata;
    std::cout << my_json << std::endl; // -> {"metadata": {"age": 23, "height": 1.750000, "is_student": true}, "name": "Axel"}
    return 0;
}
```

### Access an object's data
```cpp
#include "json.hpp"

int main()
{
    ax::Json my_json = {
        {"name", "Jane Doe"},
        {"profession", "Software Engineer"},
        {"age", 29}
    };

    int age = my_json["age"].to<int>().value_or(-1);
    double height = my_json["height"].to<double>().value_or(-1.0);

    std::cout << age << std::endl;    // -> 29
    std::cout << height << std::endl; // -> -1.0

    return 0;
}
```
### Clone an object
```cpp
#include "json.hpp"

int main()
{
    ax::Json my_json{
        {"age", 27}
    };
    ax::Json my_json_clone = my_json.clone();
    my_json_clone["name"] = "John";
    my_json_clone["is_student"] = false;
    std::cout << my_json << std::endl;       // -> {"age": 27}
    std::cout << my_json_clone << std::endl; // -> {"age": 27, "is_student": false, "name": "John"}
    return 0;
}
```
<!-- Warning! if you do not clone you will modify the original object -->
:warning: If you do not clone the object, you will modify the original object.
```cpp
#include "json.hpp"

int main()
{
    ax::Json my_json{
        {"age", 27}
    };
    ax::Json my_json_clone = my_json; // Not cloning! We are just creating a new json object that references the same underlying object!
    my_json_clone["name"] = "John";
    my_json_clone["is_student"] = false;
    std::cout << my_json << std::endl;       // -> {"age": 27, "is_student": false, "name": "John"}
    std::cout << my_json_clone << std::endl; // -> {"age": 27, "is_student": false, "name": "John"}
    return 0;
}
```