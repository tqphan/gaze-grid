FROM gazegrid-linux-check:latest
RUN pacman -Syu --noconfirm --needed qt6-virtualkeyboard > /tmp/ime-packages.log 2>&1 || (tail -n 40 /tmp/ime-packages.log; exit 1)
