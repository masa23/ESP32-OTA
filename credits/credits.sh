#!/bin/sh

set -exu

(cd ../ && pio lib list --json-output | jq . > credits/pio-libs.json)

echo "# Credits" > credits.md
echo "## ESP32 Libraries" >> credits.md

for lib in $(jq -r '.[].name' pio-libs.json); do

  # URLはRepositoryのURLを使用する
  jq -r ".[] | select(.name == \"$lib\") | \"### \(.name)@\(.version)\n\(.repository.url)\"" pio-libs.json >> credits.md
  # licenseがnull以外の時はその値を出力
  # 存在しない場合は、../.pio/libdeps/esp32dev/以下のライブラリのLICENSEファイルで判定する
  if [ "$(jq -r ".[] | select(.name == \"$lib\") | .license" pio-libs.json)" != "null" ]; then
    jq -r ".[] | select(.name == \"$lib\") | .license" pio-libs.json >> credits.md
  fi
done

(cd ../frontend && npx -y license-checker --production --json > ../credits/frontend-licenses.json)

echo "## Frontend Libraries" >> credits.md
# esp32-ota-frontend は自分自身なので、credits.mdには含めない
for lib in $(jq -r 'to_entries[] | select(.key != "esp32-ota-frontend") | .key' frontend-licenses.json); do
  echo "### $lib" >> credits.md
  jq -r ".[\"$lib\"].repository" frontend-licenses.json >> credits.md
  jq -r ".[\"$lib\"].licenses" frontend-licenses.json >> credits.md
done

rm -vf *.json
