# CSRF Protection

Cross-Site Request Forgery (CSRF) Protection in PHP is a method where a website can defend itself against unwanted actions performed on behalf of the users without their consent. It's a critical aspect of security as it safeguards users against potential harmful activities. Here's an example: if users are logged into a website and get tricked into clicking a deceitful link, CSRF attacks could be triggered. To protect your PHP applications from such attacks, you can generate a unique token for every session and include it as a hidden field for all form submissions. Afterwards, you need to verify this token on the server side before performing any action.

```
<?php
// Generate CSRF token
if(empty($_SESSION['csrf'])) {
    $_SESSION['csrf'] = bin2hex(random_bytes(32));
}

// Verify CSRF token
if(isset($_POST['csrf']) && $_POST['csrf'] === $_SESSION['csrf']) {
    // valid CSRF token, perform action
}
?>
```

Edge cases like AJAX requests and applications running across multiple domains require extra considerations. More about CSRF can be found in the [PHP Security Guide](https://php.net/manual/en/security.csrf.php).