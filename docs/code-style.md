---
pagetitle: Code Style
navbar: true
---

The ArgoDSM Programming Guidelines
==================================

## General

1. There is an 80 character soft limit. That means that preferably the lines
   should not exceed 80 characters, but longer lines are acceptable if the
   formatting makes more sense.

2. There are no specific rules for where lines should break, as long as the
   formatting makes sense.

3. The code follows the C/C++11 standards

## Spacing

1. Tabs should be used both for indentation and for alignment

2. Namespaces, as well as public/private/protected labels should be indented

## Naming

1. There is no specific way for naming classes, members, or random variables.

2. Names should be chosen so that the code is easier to understand.

## Case

1. All namespaces, classes, class members, functions, and variables should
   be in snake\_case.

2. "Magic" constants, as well as C-style enumerations that are used as integers
   should be CAPITALIZED.

3. C++-style enumerations (`enum class` etc) that are meant to be used as
   distinct tags and not as integers, should follow rule #1.

4. Template arguments should be in CamelCase.

## Braces

1. Opening braces should be attached on the same line as the statement,
   function, or class declaration. An exception to this rule is the case where
   the declaration is longer than one line, in which case the opening brace
   should be on a new line on its own.

2. Closing braces should be on a new line, with the exception of `if-else`
   statements.

3. Empty code blocks can have all their braces on the same line.

## Types

1. Pointers, references etc attach to the type (eg `int& a`)

2. `const` qualifiers should be to the left of the type.

    ```
    const int a; // A constant integer
    const int& b; // A reference to a constant integer
    ```

3. `using` expressions are preferred over `typedef` ones.

## Code Review Process

1. New code should be reviewed by at least one reviewer

2. New code should pass the whole of the test suite. The person submitting the
   new code is responsible for running the test suite. In the future, an
   automated testing framework should be used for all the pull requests.

3. Trivial bug fixes and corrections can be merged by the person making the
   pull request, without the need for someone else's approval. A pull request
   still needs to be made though.

4. There is no time limit on how long a pull request might be left open before
   being accepted.

