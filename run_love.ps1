param(
    [string]$File = "examples/01-romantic-hello.love",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$PassThruArgs
)

$bin = if (Test-Path ".\lovelang.exe") {
    ".\lovelang.exe"
} elseif (Get-Command lovelang -ErrorAction SilentlyContinue) {
    "lovelang"
} else {
    throw "Could not find lovelang.exe or lovelang on PATH. Build first with 'make'."
}

& $bin $File @PassThruArgs
exit $LASTEXITCODE
