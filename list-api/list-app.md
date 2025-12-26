
---

## Response

### Success Response (200 OK)

**Response Format:**
{
  "code": 200,
  "message": "Success",
  "data": [
    {
      "id": "app-123",
      "name": "My Application",
      "description": "Application description",
      "icon": "data:image/png;base64,iVBORw0KG...",
      "base_url": "https://app.example.com",
      "createdAt": "2025-01-16T10:00:00Z",
      "updatedAt": "2025-01-16T10:00:00Z",
      "mfa_require": true,
      "credentials": [
        {
          "id": "cred-456",
          "name": "Main Credential",
          "type": "password",
          "username": "user@example.com",
          "password": "encrypted_or_plain_password",
          "privateKey": "",
          "passphrase": "",
          "created": "2025-01-16T10:00:00Z",
          "owner": "user123",
          "encrypted": true,
          "lastPasswordChange": "2025-01-01T00:00:00Z",
          "needsChange": false,
          "daysRemaining": 30
        }
      ]
    }
  ]
}### Error Responses

**401 Unauthorized:**
{
  "code": 401,
  "message": "Unauthorized",
  "data": null
}**500 Internal Server Error:**
{
  "code": 500,
  "message": "Internal server error",
  "data": null
}---

## Data Structures

### AppPassword
| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Application ID |
| `name` | string | Application name |
| `description` | string | Mô tả application |
| `icon` | string | Icon base64 (data:image/...) |
| `base_url` | string | Base URL của application |
| `createdAt` | CustomTime | Thời gian tạo |
| `updatedAt` | CustomTime | Thời gian cập nhật |
| `mfa_require` | bool | Có yêu cầu MFA không |
| `credentials` | []Credential | Danh sách credentials |

### Credential
| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Credential ID |
| `name` | string | Tên credential |
| `type` | string | Loại (password, ssh, etc.) |
| `username` | string | Username |
| `password` | string | Password (encrypted hoặc plain) |
| `privateKey` | string | SSH private key (nếu có) |
| `passphrase` | string | Passphrase cho private key |
| `created` | string | Thời gian tạo |
| `owner` | string | Owner |
| `encrypted` | bool | Password có được mã hóa không |
| `lastPasswordChange` | CustomTime | Lần đổi password cuối |
| `needsChange` | bool | Cần đổi password không |
| `daysRemaining` | int | Số ngày còn lại |

---

## Client Processing

### Xử lý trong Go Client:

1. **Gửi request** với `X-Auth-Token` header
2. **Parse response** thành `APIResponse`
3. **Convert** `response.Data` thành `[]AppPassword`
4. **Filter icons** > 8KB để tối ưu RAM (icon quá lớn sẽ bị xóa)

### Code Example:

// URL
url := strings.TrimSuffix(c.config.APIBaseURL, "/") + "/app/agent"

// Request với auth token
resp, err := c.makeRequest("GET", url, "X-Auth-Token", c.accessToken, nil)

// Parse response
var response APIResponse
json.NewDecoder(resp.Body).Decode(&response)

// Convert data to []AppPassword
appsData, _ := json.Marshal(response.Data)
var apps []AppPassword
json.Unmarshal(appsData, &apps)

// Filter large icons (> 8KB)
for i := range apps {
    if len(apps[i].Icon) > 8192 {
        apps[i].Icon = "" // Xóa icon quá lớn
    }
}---

## Usage Examples

### cURL
curl -X GET "https://api.example.com/app/agent" \
  -H "X-Auth-Token: your-access-token-here"### Go Code
client := credentials.NewClient(config, accessToken)
apps, err := client.GetApplications()
if err != nil {
    log.Fatal(err)
}

for _, app := range apps {
    fmt.Printf("App: %s (%s)\n", app.Name, app.ID)
    fmt.Printf("  Credentials: %d\n", len(app.Credentials))
    fmt.Printf("  MFA Required: %v\n", app.MfaRequire)
}---

## Notes

1. **Authentication**: Bắt buộc có `X-Auth-Token` header
2. **Icon size**: Icons > 8KB sẽ bị loại bỏ trong client để tối ưu RAM
3. **Password encryption**: Password có thể là encrypted (`encrypted: true`) hoặc plain text (`encrypted: false`)
4. **Time format**: `CustomTime` hỗ trợ nhiều format:
   - `2006-01-02 15:04:05`
   - `2006-01-02T15:04:05Z`
   - RFC3339
5. **Multiple credentials**: Mỗi application có thể có nhiều credentials

---

## Related Functions

- `GetApplications()` - Lấy danh sách applications
- `GetApplicationCredentials(appID)` - Lấy credentials theo app ID
- `GetCredentialsByAppName(appName)` - Lấy credentials theo app name