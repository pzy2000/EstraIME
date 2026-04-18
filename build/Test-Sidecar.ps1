param(
    [int]$TimeoutMs = 2000
)

$ErrorActionPreference = "Stop"

$pipe = [System.IO.Pipes.NamedPipeClientStream]::new(".", "LOCAL/EstraIME.Sidecar".Replace("/", "\"), [System.IO.Pipes.PipeDirection]::InOut)
try {
    $pipe.Connect($TimeoutMs)

    $payload = [System.Text.Encoding]::UTF8.GetBytes('{"method":"health.get"}')
    $length = [BitConverter]::GetBytes([UInt32]$payload.Length)

    $pipe.Write($length, 0, $length.Length)
    $pipe.Write($payload, 0, $payload.Length)
    $pipe.Flush()

    $responseLengthBytes = New-Object byte[] 4
    [void]$pipe.Read($responseLengthBytes, 0, 4)
    $responseLength = [BitConverter]::ToUInt32($responseLengthBytes, 0)
    $responseBytes = New-Object byte[] $responseLength
    $read = 0
    while ($read -lt $responseLength) {
        $read += $pipe.Read($responseBytes, $read, $responseLength - $read)
    }

    $response = [System.Text.Encoding]::UTF8.GetString($responseBytes)
    Write-Host $response
}
finally {
    $pipe.Dispose()
}
