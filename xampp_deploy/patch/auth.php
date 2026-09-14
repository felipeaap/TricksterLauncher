<?php
/**
 * auth.php / launcher_auth.php
 *
 * Trickster Online Launcher Authentication API Endpoint
 *
 * Handles client login requests, verifies credentials against MySQL/MSSQL,
 * checks account status (active/banned), and returns structured JSON responses.
 */

declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('X-Content-Type-Options: nosniff');

// =============================================================================
// CONFIGURATION
// =============================================================================

// Secret token shared between launcher and server. Leave empty ("") to disable verification during development.
define('AUTH_TOKEN', '');

// Database Connection
define('DB_DRIVER',   'mysql');          // 'mysql' or 'sqlsrv'
define('DB_HOST',     '127.0.0.1');
define('DB_PORT',     3306);
define('DB_NAME',     'trickster_account');
define('DB_USER',     'root');
define('DB_PASS',     '');

// Table & Column Schema Mapping
define('TBL_ACCOUNTS',      'tbl_account'); // Table storing account records
define('COL_USERNAME',      'account_id');  // Column for user login name
define('COL_PASSWORD',      'password');    // Column for user password
define('COL_IS_BLOCKED',    'block');       // Column for ban flag (0 = active, 1 = banned) - set to null if none
define('COL_BLOCK_REASON',  'block_reason');// Column for ban reason - set to null if none
define('COL_RELEASE_DATE',  'block_date');  // Column for ban expiration - set to null if none

// Password Hashing Algorithm:
// 'md5'    - Standard classic Trickster (md5($password))
// 'sha256' - Standard SHA-256 (hash('sha256', $password))
// 'bcrypt' - Modern PHP password_hash / password_verify
// 'plain'  - Plaintext comparison (not recommended for production)
define('PASSWORD_HASH_TYPE', 'md5');

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

function respond(string $status, string $message, array $extra = []): void
{
    $payload = array_merge(['status' => $status, 'message' => $message], $extra);
    echo json_encode($payload, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

function verify_password(string $inputPassword, string $storedHash): bool
{
    switch (strtolower(PASSWORD_HASH_TYPE)) {
        case 'md5':
            return strtolower(md5($inputPassword)) === strtolower($storedHash);
        case 'sha256':
            return strtolower(hash('sha256', $inputPassword)) === strtolower($storedHash);
        case 'bcrypt':
            return password_verify($inputPassword, $storedHash);
        case 'plain':
            return $inputPassword === $storedHash;
        default:
            return false;
    }
}

// =============================================================================
// REQUEST PROCESSING
// =============================================================================

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    respond('error', 'Method not allowed. Use POST.');
}

// Support both form-urlencoded and JSON body
$username = $_POST['username'] ?? '';
$password = $_POST['password'] ?? '';
$token    = $_POST['auth_token'] ?? '';

if (empty($username) || empty($password)) {
    $rawInput = file_get_contents('php://input');
    if (!empty($rawInput)) {
        $json = json_decode($rawInput, true);
        if (is_array($json)) {
            $username = $json['username'] ?? $username;
            $password = $json['password'] ?? $password;
            $token    = $json['auth_token'] ?? $token;
        }
    }
}

$username = trim((string)$username);
$password = (string)$password;
$token    = trim((string)$token);

if (empty($username) || empty($password)) {
    respond('error', 'Username and password are required.');
}

// Validate Auth Token (if configured)
if (AUTH_TOKEN !== '' && !hash_equals(AUTH_TOKEN, $token)) {
    respond('error', 'Unauthorized: invalid auth_token.');
}

// =============================================================================
// DATABASE QUERY & AUTHENTICATION
// =============================================================================

try {
    if (DB_DRIVER === 'sqlsrv') {
        $dsn = sprintf('sqlsrv:Server=%s,%d;Database=%s', DB_HOST, DB_PORT, DB_NAME);
    } else {
        $dsn = sprintf('mysql:host=%s;port=%d;dbname=%s;charset=utf8mb4', DB_HOST, DB_PORT, DB_NAME);
    }

    $pdo = new PDO($dsn, DB_USER, DB_PASS, [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_TIMEOUT            => 5,
    ]);
} catch (PDOException $e) {
    respond('error', 'Database connection failed. Please try again later.');
}

try {
    $selectCols = [COL_USERNAME, COL_PASSWORD];
    if (COL_IS_BLOCKED !== null)   $selectCols[] = COL_IS_BLOCKED;
    if (COL_BLOCK_REASON !== null) $selectCols[] = COL_BLOCK_REASON;
    if (COL_RELEASE_DATE !== null) $selectCols[] = COL_RELEASE_DATE;

    $query = sprintf(
        'SELECT %s FROM %s WHERE %s = :username LIMIT 1',
        implode(', ', $selectCols),
        TBL_ACCOUNTS,
        COL_USERNAME
    );

    $stmt = $pdo->prepare($query);
    $stmt->execute([':username' => $username]);
    $account = $stmt->fetch();

    if (!$account) {
        respond('error', 'Invalid username or password.');
    }

    // Check Ban Status
    if (COL_IS_BLOCKED !== null && !empty($account[COL_IS_BLOCKED])) {
        $isBlocked = (int)$account[COL_IS_BLOCKED];
        if ($isBlocked !== 0) {
            $reason = (COL_BLOCK_REASON !== null && isset($account[COL_BLOCK_REASON])) ? (string)$account[COL_BLOCK_REASON] : 'Violation of Terms of Service';
            $releaseDate = (COL_RELEASE_DATE !== null && isset($account[COL_RELEASE_DATE])) ? (string)$account[COL_RELEASE_DATE] : 'Permanent';
            respond('error', 'Account is banned.', [
                'reason'       => $reason,
                'release_date' => $releaseDate
            ]);
        }
    }

    // Verify Password
    $storedPassword = (string)$account[COL_PASSWORD];
    if (!verify_password($password, $storedPassword)) {
        respond('error', 'Invalid username or password.');
    }

    // Authentication Successful!
    respond('success', 'Authentication successful.');

} catch (PDOException $e) {
    respond('error', 'Authentication query error.');
}
