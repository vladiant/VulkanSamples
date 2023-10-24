# VulkanSamples
Vulkan code samples

## Code improvement commands
```bash
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
run-clang-tidy -checks='cppcoreguidelines-*,-cppcoreguidelines-prefer-member-initializer,readibility-*,modernize-*,-modernize-use-trailing-return-type,misc-*,clang-analyzer-*' -fix
```
