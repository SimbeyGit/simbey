param(
	[ValidateSet('Debug', 'Release', 'All')]
	[string] $Configuration = 'All',
	[switch] $Build,
	[switch] $Regressions
)

$ErrorActionPreference = 'Stop'
$treeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$treeConfigurations = @('Debug', 'Release')
if ($Configuration -ne 'All')
{
	$treeConfigurations = @($Configuration)
}
$treeProjects = @(
	'desktop\QuadooStudio\CodeEdit\CodeEdit.vcproj',
	'tests\platform\QuadooForm\TestExtension\QuadooFormTestExtension.vcproj',
	'platform\QuadooForm\QuadooForm\QuadooForm.vcproj',
	'platform\QuadooForm\Designer\QuadooFormDesignerCore.vcproj',
	'platform\QuadooForm\DesignerHost\QuadooFormDesigner.vcproj',
	'tests\platform\QuadooForm\TestInternals\QuadooFormNativeTests.vcproj',
	'tests\platform\QuadooForm\Harness\QuadooFormHarness.vcproj',
	'tests\platform\QuadooForm\DesignerTests\QuadooFormDesignerTests.vcproj',
	'desktop\QuadooStudio\QuadooStudio\QuadooStudio.vcproj',
	'tests\desktop\QuadooStudio\IntegrationTests\QuadooStudioIntegrationTests.vcproj',
	'tests\desktop\QuadooStudio\ApplicationTests\QuadooApplicationTests.vcproj'
)
Push-Location $treeRoot
try
{
	foreach ($treeConfiguration in $treeConfigurations)
	{
		if ($Build)
		{
			foreach ($treeProject in $treeProjects)
			{
				& (Join-Path $treeRoot 'tests\BuildProject.cmd') $treeProject $treeConfiguration Win32
				if ($LASTEXITCODE -ne 0)
				{
					throw "TreeView build failed: $treeConfiguration $treeProject"
				}
			}
		}
		$treeOutput = Join-Path $treeRoot "tests\results\bin\$treeConfiguration"
		foreach ($treeExecutable in @('QuadooFormNativeTests.exe', 'QuadooFormHarness.exe', 'QuadooFormDesignerTests.exe', 'QuadooStudioIntegrationTests.exe'))
		{
			Write-Output "$treeConfiguration Win32 $treeExecutable --tree-only"
			& (Join-Path $treeOutput $treeExecutable) --tree-only
			if ($LASTEXITCODE -ne 0)
			{
				throw "TreeView validation failed: $treeConfiguration $treeExecutable"
			}
		}
		& (Join-Path $treeOutput 'QuadooApplicationTests.exe') --tree-only (Join-Path $treeRoot 'tests\desktop\QuadooStudio\ApplicationTests\Runtime.quadoo')
		if ($LASTEXITCODE -ne 0)
		{
			throw "TreeView script validation failed: $treeConfiguration"
		}
		if ($Regressions)
		{
			foreach ($treeExecutable in @('QuadooFormHarness.exe', 'QuadooFormDesignerTests.exe', 'QuadooStudioIntegrationTests.exe'))
			{
				& (Join-Path $treeOutput $treeExecutable) --image-list-only
				if ($LASTEXITCODE -ne 0)
				{
					throw "ImageList regression failed: $treeConfiguration $treeExecutable"
				}
			}
		}
	}
}
finally
{
	Pop-Location
}
