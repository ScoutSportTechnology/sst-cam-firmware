# Bind-mounted into the container and sourced from ~/.bashrc (wired by
# .devcontainer/script/on-create.sh). Edit on the host; changes show up live.

alias ll='ls -lah'
alias la='ls -A'
alias l='ls -CF'

# Firmware build shortcuts.
alias cfg-debug='cmake --preset debug'
alias build-debug='cmake --build --preset debug'
alias cfg-release='cmake --preset release'
alias build-release='cmake --build --preset release'
alias cfg-test='cmake --preset test'
alias build-test='cmake --build --preset test'
alias run-tests='ctest --preset test'
