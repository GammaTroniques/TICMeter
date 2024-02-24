import re

def extraire_nombre(chaine):
    match = re.match(r"\[(\d+)\]", chaine)
    if match:
        nombre = int(match.group(1))
        return nombre
    else:
        return None

string = """[2][10]ADSC[9]XXXXXXXXXXXX[9]?[13]
[10]VTIC[9]02[9]J[13]
[10]DATE[9]H240107175810[9][9]E[13]
[10]NGTF[9][32][32][32][32][32]TEMPO[32][32][32][32][32][32][9]F[13]
[10]LTARF[9][32][32][32][32]HP[32][32]BLEU[32][32][32][32][9]+[13]
[10]EAST[9]050019226[9]([13]
[10]EASF01[9]022235340[9]7[13]
[10]EASF02[9]026587270[9]H[13]
[10]EASF03[9]000425696[9]D[13]
[10]EASF04[9]000494614[9]A[13]
[10]EASF05[9]000115045[9]6[13]
[10]EASF06[9]000161261[9]8[13]
[10]EASF07[9]000000000[9]([13]
[10]EASF08[9]000000000[9])[13]
[10]EASF09[9]000000000[9]*[13]
[10]EASF10[9]000000000[9]"[13]
[10]EASD01[9]020976738[9]J[13]
[10]EASD02[9]025033735[9]=[13]
[10]EASD03[9]001799343[9]F[13]
[10]EASD04[9]002209410[9]5[13]
[10]IRMS1[9]017[9]6[13]
[10]URMS1[9]227[9]E[13]
[10]PREF[9]12[9]B[13]
[10]PCOUP[9]12[9]\[13]
[10]SINSTS[9]03900[9]R[13]
[10]SMAXSN[9]H240107024723[9]08611[9]=[13]
[10]SMAXSN-1[9]H240106033725[9]09941[9]#[13]
[10]CCASN[9]H240107173000[9]03594[9]I[13]
[10]CCASN-1[9]H240107170000[9]03668[9]&[13]
[10]UMOY1[9]H240107175000[9]225[9]2[13]
[10]STGE[9]013AC401[9]R[13]
[10]DPM2[9][32]240108060000[9]00[9]#[13]
[10]FPM2[9][32]240109060000[9]00[9]&[13]
[10]MSG1[9]PAS[32]DE[32][32][32][32][32][32][32][32][32][32]MESSAGE[32][32][32][32][32][32][32][32][32][9]<[13]
[10]PRM[9]09312300926695[9]8[13]
[10]RELAIS[9]000[9]B[13]
[10]NTARF[9]02[9]O[13]
[10]NJOURF[9]00[9]&[13]
[10]NJOURF+1[9]00[9]B[13]
[10]PJOURF+1[9]00004001[32]06004002[32]22004001[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[9].[13]
[10]PPOINTE[9]00004005[32]06004006[32]22004005[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[32]NONUTILE[9]'[13]
[3]"""

print()

output = "static const char trame[] = {"

i = 0
length = 0
while i < len(string):
    if string[i] == "\n":
        i += 1
        continue
    if string[i] == "[":
        j = i + 1
        while string[j] != "]":
            j += 1
        nombre = extraire_nombre(string[i:j+1])
        output += hex(nombre) + ","
        length +=1
        i = j
    else:
        output += hex(ord(string[i])) + ","
        length += 1
    i += 1

output = output[:-1]
output += "};"
print(output)
print("Length: " + str(length))
