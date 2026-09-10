https://github.com/user-attachments/assets/1ee4a782-afbc-4f08-845f-88c6b3453b9b


# Trickster-Launcher
## How to build:​
1. Have **Visual studio 2022 installed**;​
2. Make sure you have **Desktop C++** and **ALL the VS22 (143**) components installed;​
3. Clone this repository, and extract the downloaded .zip in a new folder;​
4. Open the **Trickster Launcher** solution;\n5. **Restore the NuGet packages**, by **right-clicking the Launcher project > Manage NuGet Packages > Restore button**;\n6. Change the configs to your liking, in the **Config.cpp** file;\n<img width="1183" height="286" alt="1756962394166- RaGEZONE" src="https://github.com/user-attachments/assets/e936f4fb-df9f-4382-99b2-215208929bbe" />
\n7. **Build** the solution, the .exe will be inside the **Output folder**;\n<img width="720" height="192" alt="1756962433454- RaGEZONE" src="https://github.com/user-attachments/assets/4fedc2a6-a4f4-4cfa-92b2-6e54a335fe89" />
\n## What do i need to send to my players?\n**EVERYTHING** that is inside the output folder (minus the .pdb file)\n
<img width="166" height="151" alt="1756963226301- RaGEZONE" src="https://github.com/user-attachments/assets/64a907b1-fe18-4cd4-a944-88d4af99b0fa" />
\n## How can i configure it in my host?\nYour update folder root should be like this:\n
<img alt="1756963308867- RaGEZONE" src="https://i.imgur.com/Hp0OfO6.png" />\n
Inside the Update folder, you put files like you would in your trickster client, **same folder structure and everything INCLUDING THE SPLASH.EXE!**\nthen you just run the **FileListGen.exe** and it is done!\n**Maintenance.txt** is simple, **true** puts the launcher in maintenance mode, **false** puts the launcher in online mode.\n