add_library(libcache
            CacheErrorCodes.h
            FileStorage.hpp
            IFlushCallback.h
            LRUCache.hpp
            LRUCacheObject.hpp
            SSARCCache.hpp
            SSARCCacheObject.hpp
            NoCache.hpp
            NoCacheObject.hpp
            ObjectFatUID.h
            VariadicNthType.h
            VolatileStorage.hpp
            PMemStorage.hpp
)
set_target_properties(libcache PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(libcache PUBLIC haldendb_compiler_flags)
