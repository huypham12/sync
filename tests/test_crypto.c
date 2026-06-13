#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/include/crypto.h"
#include "../common/include/hashmap.h"

int main() {
    printf("========== TEST CRYPTO MODULE ==========\n");
    
    // 1. Tạo file key ảo để test
    const char *key_path = "dummy_key.bin";
    FILE *f_key = fopen(key_path, "wb");
    if (f_key) {
        const char *key_data = "ThisIsA32ByteSecretKeyForAES256!";
        fwrite(key_data, 1, 32, f_key);
        fclose(f_key);
    } else {
        printf("Không thể tạo file key test.\n");
        return 1;
    }
    
    if (crypto_init(key_path) != 0) {
        printf("Khởi tạo Crypto thất bại.\n");
        return 1;
    }
    printf("[+] Crypto khởi tạo thành công!\n");

    // 2. Tạo một file plain text ảo
    const char *plain_file = "test_plain.txt";
    const char *enc_file = "test_enc.bin";
    const char *dec_file = "test_dec.txt";
    
    FILE *f_txt = fopen(plain_file, "w");
    if (f_txt) {
        fprintf(f_txt, "Xin chao, day la noi dung bi mat can dong bo qua mang!\n");
        fprintf(f_txt, "Du lieu nay se duoc ma hoa bang AES-256.\n");
        fclose(f_txt);
    }

    // 3. Test mã hoá
    printf("\n[*] Đang mã hoá file: %s -> %s\n", plain_file, enc_file);
    if (encrypt_file(plain_file, enc_file) == 0) {
        printf("[+] Mã hoá THÀNH CÔNG.\n");
    } else {
        printf("[-] Mã hoá THẤT BẠI.\n");
    }

    // 4. Test giải mã
    printf("\n[*] Đang giải mã file: %s -> %s\n", enc_file, dec_file);
    if (decrypt_file(enc_file, dec_file) == 0) {
        printf("[+] Giải mã THÀNH CÔNG.\n");
    } else {
        printf("[-] Giải mã THẤT BẠI.\n");
    }
    
    // 5. Test toàn vẹn (SHA256)
    char sha256_orig[65], sha256_dec[65];
    compute_sha256(plain_file, sha256_orig);
    compute_sha256(dec_file, sha256_dec);
    
    printf("\n[*] Kiểm tra toàn vẹn SHA-256:\n");
    printf("  - Bản gốc : %s\n", sha256_orig);
    printf("  - Bản dịch: %s\n", sha256_dec);
    
    if (strcmp(sha256_orig, sha256_dec) == 0) {
        printf("[+] Dữ liệu hoàn toàn khớp (Data integrity verified)!\n");
    } else {
        printf("[-] Dữ liệu BỊ SAI LỆCH!\n");
    }


    printf("\n========== TEST HASHMAP MODULE ==========\n");
    hashmap_init();
    
    printf("[*] Đang nạp dữ liệu vào Hashmap...\n");
    hashmap_put("file_local.txt", STATE_LOCAL);
    hashmap_put("file_network.pdf", STATE_NETWORK);
    
    printf("  - Trạng thái file_local.txt: %d (Kỳ vọng: %d)\n", hashmap_get("file_local.txt"), STATE_LOCAL);
    printf("  - Trạng thái file_network.pdf: %d (Kỳ vọng: %d)\n", hashmap_get("file_network.pdf"), STATE_NETWORK);
    printf("  - Trạng thái file_chuaco.doc: %d (Kỳ vọng: %d)\n", hashmap_get("file_chuaco.doc"), STATE_NONE);
    
    printf("\n[*] Đang xoá file_local.txt khỏi Hashmap...\n");
    hashmap_remove("file_local.txt");
    printf("  - Trạng thái file_local.txt sau khi xoá: %d (Kỳ vọng: %d)\n", hashmap_get("file_local.txt"), STATE_NONE);
    
    hashmap_destroy();
    printf("[+] Hashmap test hoàn tất.\n");
    
    printf("\n========== TẤT CẢ TEST ĐÃ XONG ==========\n");
    return 0;
}
