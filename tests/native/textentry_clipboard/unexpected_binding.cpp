#include <stdexcept>

// Kept in a separate TU: partial unrelated bindings must not make the compiler
// treat statements in unchanged actual owner methods as syntactically unreachable.
// Every unexpected call still fails, with no provider/UI or default-success path.
void RejectUnexpected(const char* message) {
    throw std::runtime_error(message);
}
