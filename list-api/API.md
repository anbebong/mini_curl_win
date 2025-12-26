# API Endpoints Documentation

## Tổng quan
Server cung cấp các RESTful API endpoints để quản lý services, config files, và các chức năng khác.

**Base URL:** `http://localhost:{HTTP_PORT}` (mặc định: `http://localhost:8766`)

**Response Format:** Tất cả API trả về JSON (trừ download file) với format:
```json
{
  "success": true/false,
  "message": "...",
  "data": { ... },
  "error": "..."
}
```

---

## 1. Health Check

### GET /health
Kiểm tra trạng thái server.

**Response:**
```json
{
  "success": true,
  "message": "Server is running"
}
```

### GET /api/health
Alias của `/health`.

---

## 2. Time

### GET /api/time
Lấy thời gian server hiện tại.

**Response:**
```json
{
  "success": true,
  "data": {
    "time": "2025-01-16T10:30:00Z"
  }
}
```

---

## 3. Clients

### GET /api/clients
Lấy số lượng clients đang kết nối TCP.

**Response:**
```json
{
  "success": true,
  "data": {
    "count": 0
  }
}
```

---

## 4. Config Management

### GET /api/config
Lấy nội dung file `agent-config.json`.

**Response:**
```json
{
  "success": true,
  "data": {
    "config": "{ ... }"
  }
}
```

**Error Response (404):**
```json
{
  "success": false,
  "error": "agent-config.json not found"
}
```

### POST /api/config
Upload file config vào server.

**Request:**
- Method: `POST`
- Content-Type: `multipart/form-data`
- Body:
  - `file`: File config (JSON format)

**Mặc định:** File sẽ được lưu thành `agent-config.json` trong thư mục `downloads/`

**Backup:** File cũ (nếu có) sẽ được backup với tên `agent-config-YYYYMMDD-HHMMSS.json.bak`

**Response (201):**
```json
{
  "success": true,
  "message": "Config file uploaded successfully: agent-config.json",
  "data": {
    "filename": "agent-config.json",
    "file_size": 1024,
    "file_path": "downloads/agent-config.json"
  }
}
```

**Error Response (400):**
```json
{
  "success": false,
  "error": "Missing 'file' field in form data"
}
```

---

## 5. Download Service

### GET /api/download?id={service_id}
Download file service theo ID từ database.

**Query Parameters:**
- `id` (required): Service ID trong database

**Response:** File download với Content-Disposition header

**Error Response (404):**
```
Service not found
```

### GET /api/download?file_path={path}
Download file theo đường dẫn trực tiếp.

**Query Parameters:**
- `file_path` (required): Đường dẫn file (phải nằm trong thư mục downloads)

**Response:** File download

**Error Response (400):**
```
Invalid file path
```

### POST /api/download
Download file service bằng JSON request.

**Request Body:**
```json
{
  "service_id": 1
}
```
hoặc
```json
{
  "service_name": "lancs-xdr",
  "version": "1.0.5",
  "os": "win64"
}
```

**Response (JSON):**
```json
{
  "success": true,
  "data": {
    "file_name": "lancs-xdr-1.0.5-win64.msi",
    "file_size": 1048576
  }
}
```

**Error Response (404):**
```json
{
  "success": false,
  "error": "File not found"
}
```

---

## 6. Update Check

### POST /api/update
Kiểm tra có bản cập nhật mới cho service không.

**Request Body:**
```json
{
  "service_name": "lancs-xdr",
  "current_version": "1.0.0",
  "os": "win64"
}
```

**Request Fields:**
- `service_name` (required): Tên service
- `current_version` (optional): Phiên bản hiện tại
- `os` (optional): OS type (win64, win32, deb64, deb32, rpm64, rpm32, macos)

**Response:**
```json
{
  "success": true,
  "data": {
    "update_available": true,
    "latest_version": "1.0.5",
    "current_version": "1.0.0"
  }
}
```

**Error Response (400):**
```json
{
  "success": false,
  "error": "service_name is required"
}
```

---

## 7. Services Management

### GET /api/services
Lấy danh sách tất cả services từ database.

**Response:**
```json
{
  "success": true,
  "data": {
    "services": [
      {
        "id": 1,
        "name": "lancs-xdr",
        "version": "1.0.5",
        "file_path": "downloads/win64/lancs-xdr-1.0.5-1.msi",
        "file_size": 1048576,
        "os": "win64",
        "created_at": "2025-01-16T10:00:00Z",
        "updated_at": "2025-01-16T10:00:00Z"
      }
    ],
    "total": 1
  }
}
```

**Error Response (503):**
```json
{
  "success": false,
  "error": "Database not available"
}
```

### POST /api/services
Upload service file mới vào hệ thống.

**Request:**
- Method: `POST`
- Content-Type: `multipart/form-data`
- Body:
  - `file` (required): File service (.exe, .msi, .deb, .rpm, .zip, .tar.gz, .tar, .dmg)
  - `service_name` (required): Tên service (phải khớp với filename)
  - `os` (required): OS type (win64, win32, deb64, deb32, rpm64, rpm32, macos)

**Validation:**
- Service name trong form phải khớp với service name được parse từ filename
- OS type phải là một trong các giá trị hợp lệ
- File sẽ được lưu vào `downloads/{os}/filename`

**Response (201):**
```json
{
  "success": true,
  "message": "Service 'lancs-xdr' (version 1.0.5, OS win64) uploaded and synced successfully",
  "data": {
    "id": 1,
    "name": "lancs-xdr",
    "version": "1.0.5",
    "file_path": "downloads/win64/lancs-xdr-1.0.5-1.msi",
    "file_size": 1048576,
    "os": "win64",
    "created_at": "2025-01-16T10:00:00Z",
    "updated_at": "2025-01-16T10:00:00Z"
  }
}
```

**Error Response (400):**
```json
{
  "success": false,
  "error": "Service name mismatch: provided 'lancs-xdr' but filename contains 'other-service'"
}
```

### GET /api/services/{id}
Lấy thông tin chi tiết của một service theo ID.

**Path Parameters:**
- `id` (required): Service ID

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "name": "lancs-xdr",
    "version": "1.0.5",
    "file_path": "downloads/win64/lancs-xdr-1.0.5-1.msi",
    "file_size": 1048576,
    "os": "win64",
    "created_at": "2025-01-16T10:00:00Z",
    "updated_at": "2025-01-16T10:00:00Z"
  }
}
```

**Error Response (404):**
```json
{
  "success": false,
  "error": "Service not found"
}
```

### PUT /api/services/{id}
Cập nhật thông tin service.

**Path Parameters:**
- `id` (required): Service ID

**Request Body:**
```json
{
  "name": "lancs-xdr",
  "version": "1.0.6",
  "file_path": "downloads/win64/lancs-xdr-1.0.6-1.msi",
  "file_size": 2097152,
  "os": "win64"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "name": "lancs-xdr",
    "version": "1.0.6",
    ...
  }
}
```

**Error Response (400):**
```json
{
  "success": false,
  "error": "Invalid request body"
}
```

### DELETE /api/services/{id}
Xóa service (file + database record).

**Path Parameters:**
- `id` (required): Service ID

**Response:**
```json
{
  "success": true,
  "message": "Service 'lancs-xdr' (ID: 1) and its file deleted successfully"
}
```

**Error Response (404):**
```json
{
  "success": false,
  "error": "Service not found in database"
}
```

**Lưu ý:** 
- API sẽ xóa file vật lý từ filesystem
- Sau đó xóa record khỏi database
- Nếu file không tồn tại nhưng record vẫn còn trong DB, vẫn sẽ xóa record

---

## 8. Help

### GET /api/help
Hiển thị danh sách các API endpoints có sẵn.

**Response:**
```json
{
  "success": true,
  "data": {
    "endpoints": {
      "GET  /api/health": "Health check",
      "GET  /api/time": "Get server time",
      "GET  /api/clients": "Get connected clients count",
      "GET  /api/config": "Get agent-config.json",
      "POST /api/config": "Upload config file",
      "GET  /api/download": "Download service file by ID or file_path",
      "POST /api/download": "Download service file (requires service_name, optional: version, os)",
      "POST /api/update": "Check for updates (requires service_name, optional: current_version, os)",
      "GET  /api/services": "Get all services",
      "POST /api/services": "Upload service file",
      "GET  /api/services/{id}": "Get service by ID",
      "PUT  /api/services/{id}": "Update service",
      "DELETE /api/services/{id}": "Delete service",
      "GET  /api/help": "Show this help"
    }
  }
}
```

---

## Tổng kết

**Tổng số endpoints:** 15 endpoints

### Phân loại theo HTTP Method:
- **GET:** 8 endpoints
- **POST:** 5 endpoints
- **PUT:** 1 endpoint
- **DELETE:** 1 endpoint

### Phân loại theo chức năng:
- **System:** Health check, Time, Clients
- **Config:** Upload/Download config file
- **Services:** CRUD operations cho services
- **Download:** Download service files
- **Update:** Kiểm tra bản cập nhật
- **Help:** API documentation

---

## Lưu ý

1. **Database:** Một số API yêu cầu database connection. Nếu database không available, API sẽ trả về lỗi 503.

2. **File Upload:** 
   - Config file: Max 10MB
   - Service file: Max 100MB

3. **OS Types:** Các giá trị hợp lệ:
   - `win64`, `win32` (Windows)
   - `deb64`, `deb32` (Debian/Ubuntu)
   - `rpm64`, `rpm32` (RPM-based Linux)
   - `macos` (macOS)

4. **File Storage:** 
   - Config files: `downloads/agent-config.json`
   - Service files: `downloads/{os}/filename`
   - Backup config: `downloads/agent-config-YYYYMMDD-HHMMSS.json.bak`

5. **Version Parsing:** Service version được tự động parse từ filename theo pattern: `service-name-version.ext`

---

**Cập nhật lần cuối:** 2025-01-16

