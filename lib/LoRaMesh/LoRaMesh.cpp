

// /**
// *@brief Calcula CRC16.
// *@param data_in: Ponteiro para o buffer contendo os dados.
// *@param length: Tamanho do buffer
// *@retval Valor de 16 bits representando o CRC16 do buffer
// fornecido. */
// #define CRC_POLY (0xA001)
// uint16_t CalcCRC(uint8_t *data_in, uint32_t length)
// {

//     uint32_t i;
//     uint8_t bitbang, j;
//     uint16_t crc_calc;
//     crc_calc = 0xC181;
//     for (i = 0; i < length; i++)
//     {
//         crc_calc ^= ((uint16_t)data_in[i]) & 0x00FF;
//         for (j = 0; j < 8; j++)
//         {
//             bitbang = crc_calc;
//             crc_calc >>= 1;
//             if (bitbang & 1)
//             {
//                 crc_calc ^= CRC_POLY;
//             }
//         }
//     }
//     return crc_calc;
// }
