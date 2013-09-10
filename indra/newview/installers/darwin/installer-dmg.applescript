-- This Applescript sets up the installer disk image: size, background image,
--   icon view, background image, icon positioning.
-- Usage: osascript installer-dmg.applescript <volume-name>
-- where <volume-name> is the volume name of the disk image.

on run (volumeName)
	tell application "Finder"
		tell disk (volumeName as string)
			open

			set theXOrigin to 400
			set theYOrigin to 100
			set theWidth to 500
			set theHeight to 600
			set iconSize to 100

			set theBottomRightX to (theXOrigin + theWidth)
			set theBottomRightY to (theYOrigin + theHeight)
			set dsStore to "\"" & "/Volumes/" & volumeName & "/" & ".DS_STORE\""

			tell container window
				set current view to icon view
				set toolbar visible to false
				set statusbar visible to false
				set the bounds to {theXOrigin, theYOrigin, theBottomRightX, theBottomRightY}
				set statusbar visible to false

				set file_list to every file
				repeat with i in file_list
					if the name of i is "Applications" then
						set the position of i to {345, 125}
					else if the name of i ends with ".app" then
						set the position of i to {145, 125}
					else if the name of i is "LGPL License.txt" then
						set the position of i to {145, 225}
					else if the name of i is "Vivox Acceptable Use Policy.txt" then
						set the position of i to {345, 225}
					end if
					-- Change the 7 to change the color: 0 is no label, then red,
					-- orange, yellow, green, blue, purple, or gray.
					set the label index of i to 7
				end repeat
			-- This close-open hack is nessesary to save positions on 10.6 Snow Leopard
			close
			open
			end tell

			set opts to the icon view options of container window
			tell opts
				set icon size to iconSize
				set arrangement to not arranged
			end tell

			set background picture of opts to file "background.png"

			update without registering applications
			-- Force saving of the size
			delay 1

			tell container window
				set statusbar visible to false
				set the bounds to {theXOrigin, theYOrigin, theBottomRightX - 10, theBottomRightY - 10}
			end tell

			update without registering applications
		end tell

		delay 1

		tell disk (volumeName as string)
			tell container window
				set statusbar visible to false
				set the bounds to {theXOrigin, theYOrigin, theBottomRightX, theBottomRightY}
			end tell

			update without registering applications
		end tell

		--give the finder some time to write the .DS_Store file
		delay 3

		set waitTime to 0
		set ejectMe to false
		repeat while ejectMe is false
			delay 1
			set waitTime to waitTime + 1

			if (do shell script "[ -f " & dsStore & " ]; echo $?") = "0" then set ejectMe to true
		end repeat
		log "waited " & waitTime & " seconds for .DS_STORE to be created."
	end tell
end run
