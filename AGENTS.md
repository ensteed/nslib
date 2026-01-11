# Code Standards

* All lower case underscore for spaces
* Do not use classes (only structs), and do not use member functions only free functions
* Avoid namespaces other than placing things in the main nslib namespace
* Only use references for const & function parameters - otherwise use pointers
* Do not use std library for anything. There is an in house array<T>, static_array<T, size>, and string that should be used
* Definitely no inheritance
* Use structs for data groupings, and functions to take actions on that data
* Do not hide things with private keyword

# Don't Count as Bugs/Issues
* If there are assertions don't warn about the "release" code path (when assertions don't crash the proram) doesn't handle the asserted case. The entire point of the assertion is to not have to handle the asserted case, and even in release builds the assertions are at least logged.
