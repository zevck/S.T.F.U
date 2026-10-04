# Machine-specific settings for Build_Local.ps1. Gitignored.

# Primary output: the MO2 mod folder the build deploys into (MO2 = AE).
$defaultOutputPath = "C:\Modding\MO2\mods\STFU - Dev"

# Other MO2 instances that get the same dev build. meta.ini is never touched,
# so each instance keeps its own MO2 metadata.
$additionalOutputPaths = @(
    "C:\Modding\FUS\mods\STFU - Dev",                              # FUS = VR
    "C:\Nolvus\Instances\Nolvus Awakening\MODS\mods\STFU - Dev"    # Nolvus = SE
)

# Creation Kit install: Pyro's --game-path. Only used when a skyrimse.ppj exists in the repo.
# $ckPath = ""

# Optional: pin a pyro.exe. Default is the VS Code papyrus-lang extension's copy.
# $pyroPath = ""

$defaultThreads = 16
