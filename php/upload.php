<?php
$targetDir = "uploads/"; // پوشه‌ای که می‌خواهید فایل‌ها در آن ذخیره شوند
$targetFile = $targetDir . basename($_FILES["file"]["name"]);
$uploadOk = 1;

// چک کنید آیا فایل واقعاً ارسال شده است
if (isset($_FILES["file"])) {
    // چک کنید آیا پوشه مقصد وجود دارد، اگر نه، آن را ایجاد کنید
    if (!is_dir($targetDir)) {
        mkdir($targetDir, 0777, true);
    }

    // تلاش برای انتقال فایل به پوشه مقصد
    if (move_uploaded_file($_FILES["file"]["tmp_name"], $targetFile)) {
        echo "The file " . basename($_FILES["file"]["name"]) . " has been uploaded.";
    } else {
        echo "Sorry, there was an error uploading your file.";
    }
} else {
    echo "No file was uploaded.";
}
?>
