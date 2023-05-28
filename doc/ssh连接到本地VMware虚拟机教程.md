因为作业需要Linux环境（wsl 2）也可以，但是我电脑的c盘空间不够，无法升级wsl，所以我有VMware虚拟机，这个环境是可以的，但是我不想在虚拟机中编程所以尝试ssh链接本地虚拟机。

首先，要设置虚拟机的网络连接方式为 NAT 模式，然后查看虚拟机的ip地址：

```bash
ifconfig
```

可能会有多个，选择e开头的那个。

然后在虚拟机中安装ssh服务：

1.命令行输入

```bash
ps -e|grep ssh
```

查看是否有sshd，如果没有，那就是没有安装openssh-server，安装一下：

```bash
sudo apt-get install openssh-server
```

2.安装后重启ssh服务：

```bash
/etc/init.d/ssh restart
```

3.修改配置文件：
```bash
sudo vim /etc/ssh/sshd_config
```

将其中 `PermitRootLogin prohibit-password` 注释掉，添加新的一行 `PermitRootLogin yes`，保存退出。

4.重启ssh服务：

```bash
/etc/init.d/ssh restart
```

然后可利用步骤一再次查看是否配置了sshd服务。

配置完成后，就可以通过：

```bash
ssh username@ip_address
```

进行 `ssh` 远程连接虚拟机了。