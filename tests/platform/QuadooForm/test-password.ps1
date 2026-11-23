param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All'
)

$ErrorActionPreference = 'Stop'
$passwordRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$passwordLogs = Join-Path $passwordRoot 'tests\results\PasswordLogs'
New-Item -ItemType Directory -Path $passwordLogs -Force | Out-Null
$passwordConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$passwordConfigurations = @($Configuration)
}

foreach ($passwordConfiguration in $passwordConfigurations)
{
	foreach ($passwordProgram in @('QuadooFormHarness', 'QuadooFormDesignerTests'))
	{
		$passwordLog = Join-Path $passwordLogs "$passwordConfiguration-$passwordProgram.log"
		$passwordError = Join-Path $passwordLogs "$passwordConfiguration-$passwordProgram-stderr.log"
		$passwordArguments = @('--password-only', '--trace')
		$passwordProcess = Start-Process -FilePath (Join-Path $passwordRoot "tests\results\bin\$passwordConfiguration\$passwordProgram.exe") `
			-ArgumentList $passwordArguments -WorkingDirectory $passwordRoot -WindowStyle Hidden `
			-RedirectStandardOutput $passwordLog -RedirectStandardError $passwordError -PassThru
		$passwordTimer = [Diagnostics.Stopwatch]::StartNew()
		while (!$passwordProcess.WaitForExit(1000))
		{
			if ($passwordTimer.Elapsed.TotalSeconds -gt 120)
			{
				$passwordProcess.Kill()
				throw "$passwordProgram timed out; see $passwordLog"
			}
		}
		$passwordProcess.WaitForExit()
		Write-Output "$passwordConfiguration $passwordProgram"
		Get-Content -LiteralPath $passwordLog -Tail 3
		if ($passwordProcess.ExitCode -ne 0)
		{
			Get-Content -LiteralPath $passwordError -Tail 8
			throw "$passwordProgram failed with exit code $($passwordProcess.ExitCode); see $passwordLog"
		}
	}
}
