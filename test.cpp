#include <iostream>
#include <map>
#include <vector>

int main() {
    // 1. Create a map to store student name → grade
    std::map<std::string, int> studentGrades;

    // 2. Create a vector to store order of insertion
    std::vector<std::string> studentNames;

    // Add some students
    studentGrades["Alice"] = 90;
    studentGrades["Bob"] = 85;
    studentGrades["Charlie"] = 78;

    // Save the insertion order in a vector
    studentNames.push_back("Alice");
    studentNames.push_back("Bob");
    studentNames.push_back("Charlie");

    // 3. Print all students with their grades (using vector to keep order)
    for (size_t i = 0; i < studentNames.size(); ++i) {
        std::string name = studentNames[i];
        int grade = studentGrades[name]; // get grade from map
        std::cout << name << " got " << grade << "/100" << std::endl;
    }

    return 0;
}

