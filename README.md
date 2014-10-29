Izumo - 日本語入力システム「いずも」
=====

Izumo Project  
日本語入力システム「いずも」は、「かんな」（Canna）をフォークしたソフトウェアです。  
現時点（2014/10/11）では、「かんな」の有する機能と変更点はありません。  


# インストール方法

以下を実行して、ビルド・インストールを行います。

$ mkdir build && cd build  
$ cmake ..  
$ make  
$ sudo make install  

# 起動方法

現在、rootユーザでのみ動作します。  
（辞書ファイルのパーミッションを変更すれば root 以外でも動作します。）  

$ izumooyashiro -r /usr/local/share/izumo/dic/  


# 使用方法

現時点では「かんな」と差異が無いため、既存のCannaクライアントが使用可能です。  
なお、libcanna 等、Cannaクライアントライブラリは含んでいませんので、  
別途インストールをお願いいたします。  


# 謝辞

「かんな」の開発に携わっている皆様に、この場をお借りして御礼を申し上げます。  
