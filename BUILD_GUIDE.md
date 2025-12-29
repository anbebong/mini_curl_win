# Hướng dẫn Build - Static vs DLL

## Sự khác biệt giữa Static Library và DLL

### Static Library (Mặc định)
- **Cách hoạt động**: Code của thư viện được compile trực tiếp vào file `.exe`
- **Kết quả**: Chỉ có 1 file `.exe`, không cần file `.dll`
- **Ưu điểm**:
  - ✅ Đơn giản: chỉ cần copy 1 file `.exe` là chạy được
  - ✅ Không phụ thuộc DLL
  - ✅ Dễ phân phối
- **Nhược điểm**:
  - ❌ File `.exe` lớn hơn
  - ❌ Nhiều ứng dụng dùng cùng thư viện → code bị duplicate trong mỗi `.exe`

### DLL (Dynamic Link Library)
- **Cách hoạt động**: Code của thư viện nằm trong file `.dll` riêng
- **Kết quả**: Có file `.exe` + file `.dll` (ví dụ: `mini_curl.dll`)
- **Ưu điểm**:
  - ✅ File `.exe` nhỏ hơn
  - ✅ Nhiều ứng dụng có thể dùng chung 1 DLL → tiết kiệm bộ nhớ
  - ✅ Có thể update DLL mà không cần rebuild `.exe`
- **Nhược điểm**:
  - ❌ Phải copy cả `.exe` và `.dll` khi phân phối
  - ❌ Phải đảm bảo DLL có sẵn khi chạy (nếu thiếu sẽ báo lỗi)

## Cách Build

### Build Static Library (Mặc định)

```bash
# Cách 1: Dùng build script
build_cmake.bat

# Cách 2: Dùng CMake trực tiếp
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

**Kết quả:**
- `build/lib/libmini_curl.a` - Static library
- `build/bin/example_simple_curl.exe` - Executable (đã link static)

### Build DLL

```bash
# Cách 1: Dùng build script với option
build_cmake.bat dll

# Cách 2: Dùng CMake trực tiếp
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=ON
cmake --build .
```

**Kết quả:**
- `build/bin/mini_curl.dll` - DLL file
- `build/bin/example_simple_curl.exe` - Executable (link với DLL)
- `build/lib/libmini_curl.dll.a` - Import library (cần khi link)

## Khi nào dùng Static? Khi nào dùng DLL?

### Dùng Static khi:
- ✅ Ứng dụng standalone, chỉ có 1 file `.exe`
- ✅ Không cần chia sẻ thư viện giữa nhiều ứng dụng
- ✅ Muốn đơn giản khi phân phối (chỉ copy 1 file)

### Dùng DLL khi:
- ✅ Nhiều ứng dụng cần dùng chung thư viện
- ✅ Muốn update thư viện mà không rebuild ứng dụng
- ✅ Muốn giảm kích thước file `.exe`
- ✅ Phát triển plugin system

## Lưu ý khi dùng DLL

1. **Copy DLL cùng với EXE:**
   ```
   build/bin/
   ├── example_simple_curl.exe
   └── mini_curl.dll          ← Phải có file này!
   ```

2. **Kiểm tra DLL có sẵn:**
   - Nếu thiếu DLL, Windows sẽ báo lỗi khi chạy `.exe`
   - Có thể đặt DLL trong cùng thư mục với `.exe` hoặc trong `PATH`

3. **Build lại khi thay đổi:**
   - Nếu thay đổi code trong thư viện, phải rebuild cả DLL và ứng dụng

