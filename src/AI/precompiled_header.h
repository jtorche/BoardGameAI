#define USE_TINY_DNN

#if defined(USE_TINY_DNN)
#pragma warning( push )
#pragma warning( disable : 4616 )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4456 )
#pragma warning( disable : 4996 )
#pragma warning( disable : 4458 )
#pragma warning( disable : 4189 )
#pragma warning( disable : 4267 )
#define CNN_SINGLE_THREAD
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <tiny_dnn/tiny_dnn.h>
#pragma warning( pop )

#else
#include <torch/torch.h>
#endif

#include <filesystem>
#include <chrono>
#include <execution>