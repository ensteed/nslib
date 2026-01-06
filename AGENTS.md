# Code Standards

* All lower case underscore for spaces
* Do not use classes (only structs), and do not use member functions only free functions
* Avoid namespaces other than placing things in the main nslib namespace
* Only use references for const & function parameters - otherwise use pointers
* Do not use std library for anything. There is an in house array<T>, static_array<T, size>, and string that should be used
* Definitely no inheritance
* Use structs for data groupings, and functions to take actions on that data
* Do not hide things with private keyword
