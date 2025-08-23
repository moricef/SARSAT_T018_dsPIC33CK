# Documentation DSSS COSPAS-SARSAT

Cette documentation analyse l'implémentation DSSS (Direct Sequence Spread Spectrum) pour les balises COSPAS-SARSAT 2ème génération selon la spécification T.018.

## 📋 Vue d'ensemble

Le système DSSS COSPAS-SARSAT utilise :
- **Modulation** : OQPSK (Offset Quadrature Phase Shift Keying)  
- **Étalement** : Séquences PRN générées par LFSR (x^23 + x^18 + 1)
- **Chip Rate** : 38.4 kchips/s
- **Symbol Rate** : 300 bps  
- **Facteur d'étalement** : 256
- **Correction d'erreurs** : BCH(255,207) raccourci à (250,202)

## 📁 Structure de la documentation

### Code MATLAB
- [`dsss_transmitter.m`](../matlab_code/dsss_transmitter.m) - Chaîne émetteur complète
- [`dsss_receiver_sync.m`](../matlab_code/dsss_receiver_sync.m) - Synchronisation récepteur  
- [`dsss_despreading.m`](../matlab_code/dsss_despreading.m) - Désétalement et décodage

### Documentation détaillée
- [Architecture émetteur](01_transmitter_architecture.md)
- [Modèle de canal RF](02_channel_model.md)
- [Synchronisation récepteur](03_receiver_synchronization.md)
- [Désétalement DSSS](04_dsss_despreading.md)
- [Correction d'erreurs BCH](05_error_correction.md)

### Validation et mapping
- [Mapping MATLAB ↔ T018](T018_mapping.md) - Correspondances implémentation
- [Guide d'implémentation](implementation_guide.md) - Adaptation pratique T018
- [Résultats validation](validation_results.md) - Métriques et performances

## 🎯 Objectif T018

Cette documentation vise à :
1. **Valider** l'implémentation T018 par rapport aux standards COSPAS-SARSAT
2. **Fournir** des références MATLAB pour développement/debug
3. **Faciliter** l'adaptation du code MATLAB vers le firmware dsPIC33CK  
4. **Documenter** les algorithmes complexes (sync, despreading, BCH)

## 🚀 Utilisation

### Pour développeurs T018
- Consulter [T018_mapping.md](T018_mapping.md) pour correspondances
- Utiliser [implementation_guide.md](implementation_guide.md) pour adaptations
- Référencer le code MATLAB pour algorithmes complexes

### Pour validation
- Exécuter les scripts MATLAB pour génération de signaux de référence
- Comparer avec sorties T018 pour validation
- Utiliser métriques de [validation_results.md](validation_results.md)

## 📊 Spécifications techniques

| Paramètre | Valeur | Unité |
|-----------|---------|-------|
| Fréquence porteuse | 406.05 | MHz |
| Chip rate | 38400 | cps |
| Symbol rate | 300 | bps |
| Spreading factor | 256 | - |
| Longueur trame | 300 | bits |
| BCH (n,k,t) | (250,202,6) | - |
| Puissance EIRP min | 33 | dBm |
| Bande passante | 76.8 | kHz |

## 🔗 Liens utiles

- **Spécification T.018** : COSPAS-SARSAT Rev.12 Oct 2024
- **Projet T018** : `/home/fab2/MPLABXProjects/SARSAT_T018_dsPIC33CK.X/`
- **Documentation MATLAB originale** : `../Matlab DSSS/`
- **Code source T018** : `../../*.c`, `../../*.h`

---
*Documentation générée automatiquement à partir des analyses MATLAB DSSS COSPAS-SARSAT*