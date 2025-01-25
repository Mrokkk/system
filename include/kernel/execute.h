#pragma once

#define execute(operation, title, ...) \
    ({ int ret = (operation); if (unlikely(ret)) { log_warning(title ": failed with %d", ret, ##__VA_ARGS__);  }; ret; })

#define execute_no_ret(operation) \
    ({ (operation); 0; })
