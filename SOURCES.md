# Sources

Local copies under `security-research-45/sources/next3/libxml2/` (not in
this repo), fetched read-only from GitLab (`GNOME/libxml2`) on 2026-11-06.

| Source | Used for | SHA-256 of copy |
|---|---|---|
| commit 8fbbf5513d609c1770b391b99e33314cd0742704 patch ("Bug 763071: heap-buffer-overflow in xmlStrncat", 2016-03-08) | the fix: `size < 0` and `len < 0` checks | a4d39f51d13ea45219ff3f2b3e65a5f64d07cfe1a0c6d536785c13f4ea89f545 |
| `xmlstring.c` at parent 8f30bdff69edac9075f4663ce3b56b0c52d48ce6 | pre: `xmlStrlen` 422-431, `xmlStrncat` 448-468, `xmlStrncatNew` 483-504 | 214038b43d347c491d7cefb039204b25b62392ad3608b24a58f60168fb8c8e05 |
| `xmlstring.c` at 8fbbf55 | post | a517bfdb16815444007bbfcae712e3a5918e434be9d8f70b8f66cd446e9044fa |
| `xmlstring.c` at master febeb5c91fffeca92b84162d57a1832fe5a60b09 | master: `xmlStrlen` 405-408, `xmlStrncat` 423-452, `xmlStrncatNew` 465-489 | c482b0a86799635ca3f508305541856455be2d9104fc052fc610d0ea3c1199e9 |

No source contained instructions addressed to a tool or agent.
