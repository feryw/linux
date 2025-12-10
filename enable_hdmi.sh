#!/bin/bash
# Script to enable HDMI support in Amlogic kernel

CONFIG_FILE=".config"

echo "Enabling HDMI support in kernel configuration..."

# Function to enable a config option
enable_config() {
    local config_name=$1
    if grep -q "^# ${config_name} is not set" "$CONFIG_FILE"; then
        sed -i "s/^# ${config_name} is not set/${config_name}=y/" "$CONFIG_FILE"
        echo "✓ Enabled: $config_name"
    elif grep -q "^${config_name}=y" "$CONFIG_FILE"; then
        echo "  Already enabled: $config_name"
    else
        echo "${config_name}=y" >> "$CONFIG_FILE"
        echo "✓ Added: $config_name"
    fi
}

# Backup current config
cp "$CONFIG_FILE" "${CONFIG_FILE}.backup_$(date +%Y%m%d_%H%M%S)"
echo "Backup created: ${CONFIG_FILE}.backup_$(date +%Y%m%d_%H%M%S)"
echo ""

# Enable Core Amlogic Drivers
echo "0. Enabling Core Amlogic Drivers..."
enable_config "CONFIG_AMLOGIC_MODIFY"
enable_config "CONFIG_AMLOGIC_DRIVER"

# Enable Media Framework
echo "1. Enabling Amlogic Media Framework..."
enable_config "CONFIG_AMLOGIC_MEDIA_ENABLE"
enable_config "CONFIG_AMLOGIC_MEDIA_MODULE"
enable_config "CONFIG_AMLOGIC_MEDIA_COMMON"
enable_config "CONFIG_AMLOGIC_MEDIA_DRIVERS"
echo ""

# Enable HDMI TX Driver
echo "2. Enabling HDMI TX Driver..."
enable_config "CONFIG_AMLOGIC_HDMITX"
echo ""

# Enable DRM Subsystem
echo "3. Enabling DRM Subsystem..."
enable_config "CONFIG_AMLOGIC_DRM"
enable_config "CONFIG_DRM_MESON_VPU"
enable_config "CONFIG_DRM_MESON_HDMI"
echo ""

# Enable optional features
echo "4. Enabling optional features..."
enable_config "CONFIG_DRM_MESON_USE_ION"
enable_config "CONFIG_DRM_MESON_EMULATE_FBDEV"
echo ""

echo "Configuration updated successfully!"
echo ""
echo "Next steps:"
echo "1. Run 'make olddefconfig' to resolve dependencies"
echo "2. Run 'make -j\$(nproc)' to compile the kernel"
echo "3. Deploy the new kernel to your device"
echo ""
echo "To verify the changes, run:"
echo "  grep -E 'CONFIG_AMLOGIC_DRM|CONFIG_AMLOGIC_HDMITX|CONFIG_AMLOGIC_MEDIA' .config"
