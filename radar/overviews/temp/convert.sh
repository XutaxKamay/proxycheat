target_path=/media/sda2/Steam/steamapps/common/Counter-Strike\ Global\ Offensive/csgo

find "$target_path/maps" -type f -name "*.bsp" -exec sh -c '
    for pathname do
        echo "$pathname"
        unzip -j "$pathname" "resource/overviews/*" -d ./
    done' sh {} +

find "$target_path/resource/overviews" -type f -name "*.dds" -exec sh -c '
    for pathname do
        cp "$pathname" ./
    done' sh {} +

find "$target_path/resource/overviews" -type f -name "*.txt" -exec sh -c '
    for pathname do
        cp "$pathname" ./
    done' sh {} +

for file in *.dds
do
file_png=$(basename "$file" .dds).png
convert "$file" "$file_png"
done
rm *.dds -f
mv *.txt ../
mv *.png ../
