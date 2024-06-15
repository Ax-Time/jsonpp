# jsonpp
A simple c++ library to handle json files.


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