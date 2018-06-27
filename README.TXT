1. 本教程基于VirtualBox(实体机也可以)+Ubuntu 16.04 LTS 64-bit(安装方法可以查阅论坛或者自行百度)
2. 安装相关依赖包(逐行复制。有些包可能用不到，但是都装上吧，以防报错){会需要输入你的帐户密码}
      sudo apt-get install git autoconf automake bash bison bzip2 diffutils file flex 
      sudo apt-get install m4 g++ gawk groff-base libncurses-dev libtool libslang2 make patch perl 
      sudo apt-get install pkg-config shtool subversion tar texinfo zlib1g zlib1g-dev git-core gettext 
      sudo apt-get install libexpat1-dev libssl-dev cvs gperf unzip python libxml-parser-perl gcc-multilib 
      sudo apt-get install gconf-editor libxml2-dev g++-4.7 g++-multilib gitk libncurses5 mtd-utils 
      sudo apt-get install libncurses5-dev libstdc++6-4.7-dev libvorbis-dev g++-4.7-multilib git autopoint 
      sudo apt-get install autogen sed build-essential intltool libelf1:i386 libglib2.0-dev xutils-dev
      sudo apt-get install lib32z1-dev lib32stdc++6
3. 将build.sh脚本中的CURPATH=/home/RT_AC1200HP/asuswrt处改为你实际存放源码的目录,并赋予执行脚本足够的权限
      sudo chmod +x build.sh
4. 运行脚本，等待编译完成，之后去/asuswrt/release/src-ra-mt7620/image查找固件(根据你存放源码的位置对应查找)
      sudo ./build.sh
