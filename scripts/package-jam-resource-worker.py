"""Build the Linux Mapping Jams resource worker for a distribution."""
import subprocess


def bundle(source, runtime):
    worker = runtime / "tools/mapping-jams/prepare-resources"
    worker.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["g++", "-std=c++17", "-O2", "-s", "-pthread",
         str(source / "native/ftebroom_jam_resource_main.cpp"),
         "-lz", "-ldl", "-o", str(worker)],
        check=True,
    )
    worker.chmod(0o755)
