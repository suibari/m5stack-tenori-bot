# An AI-Bot for M5Stack Core2

M5Stackを用いたAI会話システムにおける、表示系/VAD検出部のリポジトリです。

* ハードウェア: M5Stack Core2
* フレームワーク: Platform IO

以下動作実例。

<blockquote class="bluesky-embed" data-bluesky-uri="at://did:plc:uixgxpiqf4i63p6rgpu7ytmx/app.bsky.feed.post/3lufahsk5cc2t" data-bluesky-cid="bafyreibum5w476wkc6i6r2eyoyqn24kf5nlzzm4g2w66lv5eu34opluywq" data-bluesky-embed-color-mode="system"><p lang="ja">てのりbot Mk-3完成
ウェイクワードでbotたん～って呼びかけても起動するようになった
検出はマイコン完結なので高速

VAD(MFCC/DTW)をつかってます<br><br><a href="https://bsky.app/profile/did:plc:uixgxpiqf4i63p6rgpu7ytmx/post/3lufahsk5cc2t?ref_src=embed">[image or embed]</a></p>&mdash; すいばり@P11bコミティア153 (<a href="https://bsky.app/profile/did:plc:uixgxpiqf4i63p6rgpu7ytmx?ref_src=embed">@suibari.com</a>) <a href="https://bsky.app/profile/did:plc:uixgxpiqf4i63p6rgpu7ytmx/post/3lufahsk5cc2t?ref_src=embed">Jul 20, 2025 at 19:13</a></blockquote><script async src="https://embed.bsky.app/static/embed.js" charset="utf-8"></script>

# Install

このプログラムはあくまで表示/VAD処理/発生を役割としており、動作には別途STS(Speech To Speech)サーバが必要です。
本リポジトリをSubmoduleを含めてcloneし、Platform IOで依存ライブラリを読み込めば、ビルドできると思います。

# Usage

* Aボタン: **手動呼びかけボタン**。タッチしている間話しかけられる。離すとAIから応答
* Bボタン: **キャンセルボタン**。録音や再生を中断し、初期状態に戻る
* Cボタン: **ウェイクワード登録ボタン**。画面切り替わり後、話しかけたワードがウェイクワードとなる(最大1つ)。

# Acknowledgements

以下プロジェクトを修正して利用しております。まことにありがとうございます。

SimpleVox
https://github.com/MechaUma/SimpleVox
