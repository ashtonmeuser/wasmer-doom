#define ERR_GENERIC 1
#define FAIL(message, ret) do { std::cerr << "Error: " << message << std::endl;; return ret; } while (0)
#define FAIL_IF(cond, message, ret) if (cond) FAIL(message, ret)