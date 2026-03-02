          mkdir -p ./UnleashedRecompLib/private

          handle_url() {
            local url="$1"
            local target_name="$2"
            local temp_file="${{ runner.temp }}/$(basename "$target_name")_data"

            if [ -z "$url" ]; then
              echo "Warning: No URL provided for $target_name"
              return 0
            fi

            echo "Downloading $target_name from URL (masked for safety)..."
            echo "::add-mask::$url"

            if ! wget --no-check-certificate --content-disposition -qO "$temp_file" "$url"; then
              echo "Error: Failed to download $target_name"
              return 1
            fi

            if [ ! -s "$temp_file" ]; then
              echo "Error: Downloaded file for $target_name is empty!"
              rm -f "$temp_file"
              return 1
            fi

            # Check for archive type
            if 7z l "$temp_file" > /dev/null 2>&1; then
               echo "Detected archive for $target_name, extracting..."
               if ! 7z x -y "$temp_file" -o./UnleashedRecompLib/private/; then
                 echo "Error: Failed to extract $target_name archive"
                 rm -f "$temp_file"
                 return 1
               fi
               rm -f "$temp_file"
            else
               # Check for ISO or raw file
               local file_type=$(file "$temp_file")
               if echo "$file_type" | grep -qi "iso"; then
                  echo "Detected ISO file for $target_name, extracting contents..."
                  if ! 7z x -y "$temp_file" -o./UnleashedRecompLib/private/; then
                    echo "Warning: Failed to extract ISO contents cleanly, proceeding anyway..."
                  fi
                  rm -f "$temp_file"
               else
                  case "$target_name" in
                    game)
                      echo "Assuming raw XEX file for game"
                      mv "$temp_file" ./UnleashedRecompLib/private/default.xex
                      ;;
                    *)
                      echo "Using raw file as $target_name"
                      mv "$temp_file" "./UnleashedRecompLib/private/$target_name"
                      ;;
                  esac
               fi
            fi
            return 0
          }

          # Download all provided URLs
          handle_url "$GAME_URL" "game" || exit 1
          handle_url "$UPDATE_URL" "default.xexp" || exit 1
          handle_url "$DLC_URL" "shader.ar" || exit 1

          # Handle data from private repo if checked out
          if [ -d "./private" ]; then
            echo "Copying data from private repository..."
            find ./private -maxdepth 5 -type f -iname "default.xex" -exec cp -f {} ./UnleashedRecompLib/private/default.xex \; 2>/dev/null || true
            find ./private -maxdepth 5 -type f -iname "default.xexp" -exec cp -f {} ./UnleashedRecompLib/private/default.xexp \; 2>/dev/null || true
            find ./private -maxdepth 5 -type f -iname "shader.ar" -exec cp -f {} ./UnleashedRecompLib/private/shader.ar \; 2>/dev/null || true
          fi

          # Recursive extraction: check if we extracted an ISO, another archive, or STFS container
            echo "Current contents of ./UnleashedRecompLib/private/ before finalization:"
            ls -R ./UnleashedRecompLib/private/
          echo "Checking for nested archives/ISOs/STFS in ./UnleashedRecompLib/private/..."
          # Max 5 levels of nesting
          for i in {1..5}; do
            # Find files that look like archives or STFS
            nested=$(find ./UnleashedRecompLib/private -type f \( -iname "*.iso" -o -iname "*.7z" -o -iname "*.zip" -o -iname "*.rar" -o -iname "TU_*" -o -iname "*.tar*" -o -iregex ".*/[0-9A-F]\{8,42\}" \) -not -name "*.skipped" -not -name "*.extracted" | head -n 1)

            if [ -z "$nested" ]; then
              break
            fi

            echo "Processing nested item (iteration $i): $nested"
            ls -R ./UnleashedRecompLib/private/

            # Use extension to decide extractor
            filename=$(basename "$nested")
            extension="${filename##*.}"
            is_iso=0
            if [ "$extension" == "iso" ] || [ "$extension" == "ISO" ]; then
                is_iso=1
            fi

            if 7z x -y "$nested" -o./UnleashedRecompLib/private/; then
              echo "Successfully extracted with 7z: $nested"
              mv "$nested" "${nested}.extracted"
            elif [ "$is_iso" -eq 0 ] && python3 tools/extract_stfs.py "$nested" ./UnleashedRecompLib/private/; then
              echo "Successfully extracted with extract_stfs.py: $nested"
              mv "$nested" "${nested}.extracted"
            else
              if [ "$is_iso" -eq 1 ]; then
                 echo "Warning: 7z failed to extract ISO $nested. It might be an XGD (Xbox Game Disc) image which requires specialized tools like extract-xiso."
              else
                 echo "Warning: Failed to extract $nested, skipping..."
              fi
              mv "$nested" "${nested}.skipped"
            fi
          done

          # Final search and move to ensure files are at the root of private/ and lowercase
          echo "Organizing and normalizing files in ./UnleashedRecompLib/private/..."

          echo "Current contents of ./UnleashedRecompLib/private/ before finalization:"
          ls -R ./UnleashedRecompLib/private/
          finalize_file() {
            local filename="$1"
            local target_path="./UnleashedRecompLib/private/$filename"

            # Find the file case-insensitively, anywhere in private/, as a file
            local found_file=$(find ./UnleashedRecompLib/private -type f -iname "$filename" | grep -vE "^$target_path$" | head -n 1)

            if [ -n "$found_file" ]; then
              echo "Found $filename at $found_file, moving to root as $filename..."
              mv -f "$found_file" "$target_path"
            fi

            # If it's already at the root but with wrong case, fix it
            local root_file=$(find ./UnleashedRecompLib/private -maxdepth 1 -type f -iname "$filename" | head -n 1)
            if [ -n "$root_file" ] && [ "$root_file" != "$target_path" ]; then
                 echo "Fixing case for $root_file -> $target_path"
                 mv -f "$root_file" "$target_path"
            fi
          }

          finalize_file "default.xex"
          finalize_file "default.xexp"
          finalize_file "shader.ar"

          # Verification and diagnostics
          echo "Verifying mandatory assets..."
          missing_mandatory=0
          if [ ! -f "./UnleashedRecompLib/private/default.xex" ]; then
            echo "::error::Mandatory file default.xex not found!"
            missing_mandatory=1
          fi
          if [ ! -f "./UnleashedRecompLib/private/shader.ar" ]; then
            echo "::error::Mandatory file shader.ar not found!"
            missing_mandatory=1
          fi
