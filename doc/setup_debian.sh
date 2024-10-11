# Add lunarG Vulkan repository
wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list

#install dependencies
# Read each line from the input file and install the package
input_file="./dependencies_debian.txt"
while IFS= read -r package; do
    sudo apt-get install -y "$package"
done < "$input_file"