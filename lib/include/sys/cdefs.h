#pragma once

#ifdef __cplusplus
#    ifndef __BEGIN_DECLS
#        define __BEGIN_DECLS extern "C" {
#        define __END_DECLS }
#    endif
#else
#    ifndef __BEGIN_DECLS
#        define __BEGIN_DECLS
#        define __END_DECLS
#    endif
#endif
