# 如何使用openssl生成证书及签名

## 第一步，生成私钥

$ openssl genrsa -out privatekey.pem 2048
查看生成的私钥内容

$ file privatekey.pem 
privatekey.pem: PEM RSA private key

$ cat privatekey.pem
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEA8AWq2V3g4B9fN7Tj37k0Wmut70ylRyziebyE3baA24pgixgu
8wpXztHdF5YixjbOdLvaqGQ3ck1CPRMD+cB3awgfw+/jPJqzdg2ACa9IFkIM5eaH
...
Zvib8+BsiAoiqXr4vAi8Lb64TJv3JDwOKEH/dnpXVmsDEt3wKRWX5A==
-----END RSA PRIVATE KEY-----

另外可以用openssl命令查看私钥的明细：

$ openssl rsa -in privatekey.pem -noout -text
Private-Key: (2048 bit)
modulus:
...

## 第二步，由私钥生产对应的公钥

$ openssl rsa -in privatekey.pem -pubout -out publickey.pem
查看生成的公钥内容

$ file publickey.pem 
publickey.pem: ASCII text

$ cat publickey.pem 
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA8AWq2V3g4B9fN7Tj37k0
...
vQIDAQAB
-----END PUBLIC KEY-----

另外，也可以使用openssl命令查看公钥的明细：

$ openssl rsa -pubin -in publickey.pem -noout -text
Public-Key: (2048 bit)
Modulus:
...

## 第三步，根据私钥生成证书签名请求

$ openssl req -new -key privatekey.pem -out csr.pem -subj "/C=CN/ST=BJ/L=BJ/O=HD/OU=dev/CN=hello/emailAddress=hello@world.com"

查看证书请求文件的内容：

$ file csr.pem 
csr.pem: PEM certificate request
$ cat csr.pem 
-----BEGIN CERTIFICATE REQUEST-----
MIICvjCCAaYCAQAweTELMAkGA1UEBhMCQ04xCzAJBgNVBAgMAkJKMQswCQYDVQQH
...
c8L1GiAnIN8bXSWpZT2ZfHcnVbYvz4bgxFGTncA06JwDHw==
-----END CERTIFICATE REQUEST-----
也可以通过openssl命令查看证书请求文件的明细。

$ openssl req -noout -text -in csr.pem
Certificate Request:
    Data:
        Version: 0 (0x0)
        Subject: C=CN, ST=BJ, L=BJ, O=HD, OU=dev, CN=hello/emailAddress=hello@world.com
        Subject Public Key Info:
            Public Key Algorithm: rsaEncryption
                Public-Key: (2048 bit)
                Modulus:
                ...

## 第四步，发送签发请求到CA进行签发，生成 x509证书

这里我们没有CA服务器，所以需要假装生成一个CA服务器

### 4.1 生成CA私钥

$ openssl genrsa -out ca.key 2048

### 4.2 根据CA私钥生成CA的自签名证书

$ openssl req -new -x509 -days 365 -key ca.key -out ca.crt -subj "/C=CN/ST=BJ/L=BJ/O=HD/OU=dev/CN=ca/emailAddress=ca@world.com"
注意这一步和前面第三步的区别，这一步直接生成自签名的证书，而在第三步生成的是证书签名请求，这个证书签名请求是要发给CA生成最终证书的。

查看自签名的CA证书：

$ file ca.crt 
ca.crt: PEM certificate
$ openssl x509 -in ca.crt -noout -text   
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number:
            8a:6e:10:c5:f6:18:f7:67
    Signature Algorithm: sha256WithRSAEncryption
        Issuer: C=CN, ST=BJ, L=BJ, O=HD, OU=dev, CN=ca/emailAddress=ca@world.com
        Validity
            Not Before: May 26 00:36:39 2018 GMT
            Not After : May 26 00:36:39 2019 GMT
        Subject: C=CN, ST=BJ, L=BJ, O=HD, OU=dev, CN=ca/emailAddress=ca@world.com
        Subject Public Key Info:
            Public Key Algorithm: rsaEncryption
                Public-Key: (2048 bit)
                Modulus:
                    00:cf:0c:6b:ed:2a:d7:28:55:a2:54:5a:78:1c:6a:
                    ...
                    cb:c5
                Exponent: 65537 (0x10001)
        X509v3 extensions:
            X509v3 Subject Key Identifier: 
                6E:00:06:26:92:A0:02:66:73:8C:A9:7E:47:DC:EB:A2:3F:91:F7:BC
            X509v3 Authority Key Identifier: 
                keyid:6E:00:06:26:92:A0:02:66:73:8C:A9:7E:47:DC:EB:A2:3F:91:F7:BC

            X509v3 Basic Constraints: 
                CA:TRUE
    Signature Algorithm: sha256WithRSAEncryption
         bc:d7:92:12:56:30:10:a8:b3:cf:b0:0d:7c:52:79:7b:22:2a:
         ...
         e5:11:28:99
### 4.3 使用CA的私钥和证书对用户证书签名

$ openssl x509 -req -days 3650 -in csr.pem -CA ca.crt -CAkey ca.key -CAcreateserial -out crt.pem
查看生成证书内容

$ file crt.pem 
crt.pem: PEM certificate
$ cat crt.pem 
-----BEGIN CERTIFICATE-----
MIIDaTCCAlECCQDzYtuYa7OlUTANBgkqhkiG9w0BAQsFADB0MQswCQYDVQQGEwJD
...
Zo7/JmQs
tCqjMPMc1lPuS3zmHg==
-----END CERTIFICATE-----
$ openssl x509 -in crt.pem -noout -text
Certificate:
    Data:
        Version: 1 (0x0)
        Serial Number:
            f3:62:db:98:6b:b3:a5:51
    Signature Algorithm: sha256WithRSAEncryption
        Issuer: C=CN, ST=BJ, L=BJ, O=HD, OU=dev, CN=ca/emailAddress=ca@world.com
        Validity
            Not Before: May 26 00:40:35 2018 GMT
            Not After : May 23 00:40:35 2028 GMT
        Subject: C=CN, ST=BJ, L=BJ, O=HD, OU=dev, CN=hello/emailAddress=hello@world.com
        Subject Public Key Info:
            Public Key Algorithm: rsaEncryption
                Public-Key: (2048 bit)
                Modulus:
                    00:b7:7b:c3:e4:12:65:b9:1d:04:8b:6d:b2:f4:ff:
                    ...
                    e3:bd
                Exponent: 65537 (0x10001)
    Signature Algorithm: sha256WithRSAEncryption
         8e:5f:5e:f3:fa:8a:bf:e4:7f:e1:84:99:24:3d:a6:86:ce:db:
         ...
         4b:7c:e6:1e

### 4.4 什么是消息签名

对消息签名简单地说分为三部分：

针对消息内容生成一个哈希值
使用私钥对生成的哈希值进行加密
然后把加密后的哈希值和你签名过的证书添加到消息块中。
当用户收到消息后，首先使用签名证书里的公钥对收到的加密后的哈希值进行解密，然后再对消息内容也生成一边哈希值，通过比较两个哈希值是否一致。


From：https://www.jianshu.com/p/7d940d7a07d9
