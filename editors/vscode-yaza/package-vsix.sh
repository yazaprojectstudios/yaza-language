#!/bin/sh
set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR"

VERSION="1.0.0"
VSIX_NAME="yaza-language-support-$VERSION.vsix"
OUTPUT_DIR="$SCRIPT_DIR/../../dist"
WEB_RELEASES="$SCRIPT_DIR/../../../website/public/releases"
STAGING="$SCRIPT_DIR/dist-vsix"

printf "==> Building VS Code extension package %s...\n" "$VSIX_NAME"

rm -rf "$STAGING"
mkdir -p "$STAGING/extension" "$OUTPUT_DIR"

# Copy extension files into extension/ directory
cp package.json "$STAGING/extension/"
cp extension.js "$STAGING/extension/"
cp formatter.js "$STAGING/extension/"
cp language-configuration.json "$STAGING/extension/"
cp README.md "$STAGING/extension/"
cp ../../LICENSE "$STAGING/extension/LICENSE.txt"
cp -R snippets "$STAGING/extension/"
cp -R syntaxes "$STAGING/extension/"
cp -R images "$STAGING/extension/"

# Create [Content_Types].xml
cat << 'EOF' > "$STAGING/[Content_Types].xml"
<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="json" ContentType="application/json"/>
  <Default Extension="js" ContentType="application/javascript"/>
  <Default Extension="md" ContentType="text/markdown"/>
  <Default Extension="png" ContentType="image/png"/>
  <Default Extension="txt" ContentType="text/plain"/>
  <Default Extension="vsixmanifest" ContentType="text/xml"/>
  <Default Extension="xml" ContentType="text/xml"/>
</Types>
EOF

# Create extension.vsixmanifest
cat << EOF > "$STAGING/extension.vsixmanifest"
<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata>
    <Identity Id="yaza-language-support" Version="$VERSION" Publisher="yaza-language"/>
    <DisplayName>YAZA Language Support</DisplayName>
    <Description>YAZA syntax, formatting, compiler diagnostics, completion, navigation, and snippets.</Description>
    <Icon>extension/images/yaza-logo.png</Icon>
    <License>extension/LICENSE.txt</License>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code"/>
  </Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Content.License" Path="extension/LICENSE.txt" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Icons.Default" Path="extension/images/yaza-logo.png" Addressable="true"/>
  </Assets>
</PackageManifest>
EOF

# Create zip archive as .vsix without extra fields (required by Open VSX & Marketplace)
rm -f "$OUTPUT_DIR/$VSIX_NAME" "$OUTPUT_DIR/yaza-latest.vsix"
find "$STAGING" -name '.DS_Store' -type f -delete 2>/dev/null || true
cd "$STAGING"
zip -X -qr "$OUTPUT_DIR/$VSIX_NAME" "[Content_Types].xml" "extension.vsixmanifest" "extension"
cp "$OUTPUT_DIR/$VSIX_NAME" "$OUTPUT_DIR/yaza-latest.vsix"
if [ -d "$SCRIPT_DIR/../../../website/public" ]; then
    mkdir -p "$WEB_RELEASES"
    cp "$OUTPUT_DIR/$VSIX_NAME" "$WEB_RELEASES/$VSIX_NAME"
    cp "$OUTPUT_DIR/$VSIX_NAME" "$WEB_RELEASES/yaza-latest.vsix"
fi
rm -rf "$STAGING"

printf "  [OK] Built VS Code VSIX -> %s/%s\n" "$OUTPUT_DIR" "$VSIX_NAME"
