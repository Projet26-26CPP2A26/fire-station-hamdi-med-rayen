#pragma once
// ===== databasemanager =====

#include <QSqlDatabase>
#include <QWidget>

class DatabaseManager
{
public:
    static bool connectInteractive(QWidget *parent);
    static bool isConnected();
    static QSqlDatabase database();
    static void close();
private:
    static QSqlDatabase s_db;
    static bool s_connected;
};

// ===== styleutils =====

#include <QList>
#include <QStyle>
#include <QWidget>

// Les propriétés class="..." (ex : class="btnPrimary") ne sont prises en compte par style.qss
// qu'après un "re-polish" : on demande au style de se réappliquer sur chaque widget.
inline void repolish(QWidget *fenetre)
{
    QList<QWidget *> widgets = fenetre->findChildren<QWidget *>();
    for (int i = 0; i < widgets.size(); i++) {
        widgets[i]->style()->unpolish(widgets[i]);
        widgets[i]->style()->polish(widgets[i]);
    }
    fenetre->style()->unpolish(fenetre);
    fenetre->style()->polish(fenetre);
}


// ===== pdfexport =====

#include <QList>
#include <QString>
#include <QStringList>

// Crée un fichier PDF contenant un titre et un tableau. Retourne true si le fichier est créé.
bool creerPdfTableau(const QString &chemin, const QString &titre,
                     const QStringList &entetes, const QList<QStringList> &lignes);


// ===== personnel =====

#include <QString>
#include <QStringList>

// Entité "Personnel" (Module 1 : Gestion du personnel)
class Personnel
{
public:
    Personnel();
    Personnel(const QString &id, const QString &nom, const QString &prenom,
            const QString &grade, const QString &poste, const QString &telephone,
            const QString &email, const QString &motDePasse,
            const QString &disponibilite, int missionsEnCours);

    // Getters
    QString id() const { return m_id; }
    QString nom() const { return m_nom; }
    QString prenom() const { return m_prenom; }
    QString grade() const { return m_grade; }
    QString poste() const { return m_poste; }
    QString telephone() const { return m_telephone; }
    QString email() const { return m_email; }
    QString motDePasse() const { return m_motDePasse; }
    QString disponibilite() const { return m_disponibilite; }
    int missionsEnCours() const { return m_missionsEnCours; }

    // Setters
    void setId(const QString &v) { m_id = v; }
    void setNom(const QString &v) { m_nom = v; }
    void setPrenom(const QString &v) { m_prenom = v; }
    void setGrade(const QString &v) { m_grade = v; }
    void setPoste(const QString &v) { m_poste = v; }
    void setTelephone(const QString &v) { m_telephone = v; }
    void setEmail(const QString &v) { m_email = v; }
    void setMotDePasse(const QString &v) { m_motDePasse = v; }
    void setDisponibilite(const QString &v) { m_disponibilite = v; }
    void setMissionsEnCours(int v) { m_missionsEnCours = v; }

    // Métier : charge opérationnelle en % (chaque mission en cours = 20 %, maximum 100 %)
    int charge() const;
    QString nomComplet() const;

    // Listes de valeurs autorisées
    static QStringList grades();
    static QStringList postes();
    static QStringList disponibilites();
    static int rangGrade(const QString &grade);

private:
    QString m_id;
    QString m_nom;
    QString m_prenom;
    QString m_grade;
    QString m_poste;
    QString m_telephone;
    QString m_email;
    QString m_motDePasse;
    QString m_disponibilite;
    int m_missionsEnCours;
};


// ===== vehicule =====

#include <QDate>
#include <QString>
#include <QStringList>

// Entité "Véhicule" (Module 2 : Gestion des véhicules)
class Vehicule
{
public:
    Vehicule();
    Vehicule(const QString &id, const QString &immatriculation, const QString &type,
             const QString &modele, const QString &etat, const QString &disponibilite,
             const QDate &dateMaintenance);

    // Getters
    QString id() const { return m_id; }
    QString immatriculation() const { return m_immatriculation; }
    QString type() const { return m_type; }
    QString modele() const { return m_modele; }
    QString etat() const { return m_etat; }
    QString disponibilite() const { return m_disponibilite; }
    QDate dateMaintenance() const { return m_dateMaintenance; }

    // Setters
    void setId(const QString &v) { m_id = v; }
    void setImmatriculation(const QString &v) { m_immatriculation = v; }
    void setType(const QString &v) { m_type = v; }
    void setModele(const QString &v) { m_modele = v; }
    void setEtat(const QString &v) { m_etat = v; }
    void setDisponibilite(const QString &v) { m_disponibilite = v; }
    void setDateMaintenance(const QDate &v) { m_dateMaintenance = v; }

    // Vrai si la maintenance est prévue dans les 30 prochains jours (ou déjà dépassée)
    bool maintenanceProche() const;

    // Listes de valeurs autorisées
    static QStringList types();
    static QStringList etats();
    static QStringList disponibilites();
    static QStringList typesPourIntervention(const QString &intervention);

private:
    QString m_id;
    QString m_immatriculation;
    QString m_type;
    QString m_modele;
    QString m_etat;
    QString m_disponibilite;
    QDate m_dateMaintenance;
};


// ===== piechartwidget =====
#include <QWidget>
#include <QStringList>
#include <QList>
class PieChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PieChartWidget(const QString &centerLabel, QWidget *parent = 0);
    void setData(const QStringList &labels, const QList<int> &values);
protected:
    void paintEvent(QPaintEvent *event);
private:
    QString m_centerLabel;
    QStringList m_labels;
    QList<int> m_values;
};

// ===== statsdialog =====

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QComboBox;
class QGridLayout;

// Un critère de statistique. Exemple : nom = "Grade",
// etiquettes = Capitaine, Sergent...  et valeurs = 1, 2...
struct StatCritere
{
    QString nom;
    QStringList etiquettes;
    QList<int> valeurs;
};

// Compte, pour chaque valeur possible, combien de fois elle apparaît dans 'reelles'
StatCritere compterParValeur(const QString &nom, const QStringList &possibles,
                             const QStringList &reelles);

// Fenêtre de statistiques graphiques avec diagramme circulaire (donut) et pourcentages
class StatsDialog : public QDialog
{
    Q_OBJECT
public:
    StatsDialog(const QString &titre, const QList<StatCritere> &criteres, QWidget *parent = 0);

private slots:
    void afficherCritere(int index);

private:
    QList<StatCritere> m_criteres;
    QComboBox *m_combo;
    QGridLayout *m_grille;
};


// ===== personneldialog =====

#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;

// Formulaire d'ajout / modification d'un personnel (créé en code : pas de .ui supplémentaire)
class PersonnelDialog : public QDialog
{
    Q_OBJECT
public:
    PersonnelDialog(const QString &titre, const QString &id, QWidget *parent = 0);

    void setPersonnel(const Personnel &p); // remplit le formulaire
    Personnel personnel() const;           // lit le formulaire

private slots:
    void valider(); // vérifie la saisie puis ferme le formulaire

private:
    QString m_id;
    QLineEdit *m_nom;
    QLineEdit *m_prenom;
    QComboBox *m_grade;
    QComboBox *m_poste;
    QLineEdit *m_telephone;
    QLineEdit *m_email;
    QLineEdit *m_motDePasse;
    QComboBox *m_disponibilite;
    QSpinBox *m_missions;
};


// ===== vehiculedialog =====

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QComboBox;
class QDateEdit;

// Formulaire d'ajout / modification d'un véhicule (créé en code : pas de .ui supplémentaire)
class VehiculeDialog : public QDialog
{
    Q_OBJECT
public:
    // 'immatriculationsExistantes' sert à refuser les doublons
    VehiculeDialog(const QString &titre, const QString &id,
                   const QStringList &immatriculationsExistantes, QWidget *parent = 0);

    void setVehicule(const Vehicule &v); // remplit le formulaire
    Vehicule vehicule() const;           // lit le formulaire

private slots:
    void valider(); // vérifie la saisie puis ferme le formulaire

private:
    QString m_id;
    QStringList m_existantes;
    QLineEdit *m_immatriculation;
    QComboBox *m_type;
    QLineEdit *m_modele;
    QComboBox *m_etat;
    QComboBox *m_disponibilite;
    QDateEdit *m_dateMaintenance;
};


// ===== gsmartfirestation =====

#include <QColor>
#include <QList>
#include <QMainWindow>


class QLabel;
class QProgressBar;
class QTableWidget;
class QTableWidgetItem;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QGroupBox;
class QWidget;
class QDateEdit;
class PieChartWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class GSmartFireStation;
}
QT_END_NAMESPACE

// Fenêtre principale (G = Gestion). UN SEUL fichier .ui : les vues sont les pages
// d'un QStackedWidget (page 0 = Dashboard, 1 = Personnel, 2 = Véhicules).
//
// Intégration : le code de chaque module vient de son projet individuel (copier-coller) :
//   gsmartfirestation_personnel.cpp  = code du projet Personnel
//   gsmartfirestation_vehicules.cpp = code du projet Vehicules
class GSmartFireStation : public QMainWindow
{
    Q_OBJECT

public:
    GSmartFireStation(QWidget *parent = 0);
    ~GSmartFireStation();
    void setInitialPage(int index) { goToPage(index); }

private slots:
    // ----- Navigation entre les pages -----
    void on_btnNavDashboard_clicked();
    void on_btnNavPersonnel_clicked();
    void on_btnNavVehicules_clicked();
    void on_btnGererPersonnel_clicked();
    void on_btnGererVehicules_clicked();
    void on_btnRetourPersonnel_clicked();
    void on_btnRetourVehicule_clicked();
    void on_btnQuitter_clicked();
    void on_btnVoirStats_clicked();

    // ----- Module Personnel (slots créés automatiquement par le nom on_<objet>_<signal>) -----
    void on_btnAjouterPersonnel_clicked();
    void on_btnModifierPersonnel_clicked();
    void on_btnSupprimerPersonnel_clicked();
    void on_btnRechercherPersonnel_clicked();
    void on_lineEditRecherchePersonnel_textChanged(const QString &texte);
    void on_btnTrierPersonnel_clicked();
    void on_btnExportPersonnel_clicked();
    void on_btnStatsPersonnel_clicked();
    void on_tableWidgetPersonnel_cellDoubleClicked(int ligne, int colonne);
    void on_spinSeuil_valueChanged(int valeur);
    void on_btnDetailsSurcharge_clicked();
    void enregistrerPersonnelInline();
    void annulerPersonnelInline();

    // ----- Module Véhicules (slots créés automatiquement par le nom on_<objet>_<signal>) -----
    void on_btnAjouterVehicule_clicked();
    void on_btnModifierVehicule_clicked();
    void on_btnSupprimerVehicule_clicked();
    void on_btnRechercherVehicule_clicked();
    void on_lineEditRechercheVehicule_textChanged(const QString &texte);
    void on_btnTrierVehicule_clicked();
    void on_btnExportVehicule_clicked();
    void on_btnStatsVehicule_clicked();
    void on_tableWidgetVehicules_cellDoubleClicked(int ligne, int colonne);
    void on_comboIntervention_currentIndexChanged(int index);
    void on_btnUtiliserVehicule_clicked();
    void enregistrerVehiculeInline();
    void annulerVehiculeInline();

private:
    // ----- Navigation + Dashboard -----
    void goToPage(int index);
    void refreshDashboard();
    void majBarre(QProgressBar *barre, QLabel *label, int valeur, int total);
    void ajouterAlerte(const QString &texte, const QString &couleur);

    // ----- Module Personnel -----
    void setupPersonnel();
    void chargerPersonnelExemple();
    void refreshPersonnelTable();
    void refreshSurcharge();
    void filtrerPersonnel();
    int personnelSelectionne();
    QString prochainIdPersonnel();
    bool doitEchangerPersonnel(Personnel a, Personnel b, int critere);
    StatCritere statPersonnelParGrade();
    StatCritere statPersonnelParDispo();
    QList<Personnel> m_personnel;
    QGroupBox *m_formPersonnel;
    QLineEdit *m_formNom;
    QLineEdit *m_formPrenom;
    QLineEdit *m_formTelephone;
    QLineEdit *m_formEmail;
    QLineEdit *m_formMotDePasse;
    QComboBox *m_formGrade;
    QComboBox *m_formPoste;
    QComboBox *m_formDisponibilite;
    QSpinBox *m_formMissions;
    QLabel *m_formId;
    int m_personnelEditIndex;
    QWidget *m_personnelStatsChart;
    QLabel *m_personnelStatsLegend;
    QLineEdit *m_vehicleImmat;
    QComboBox *m_vehicleType;
    QLineEdit *m_vehicleModele;
    QComboBox *m_vehicleEtat;
    QComboBox *m_vehicleDispo;
    QDateEdit *m_vehicleDate;
    QLabel *m_vehicleId;
    QGroupBox *m_formVehicule;
    int m_vehiculeEditIndex;
    QWidget *m_vehiculeStatsChart;

    // ----- Module Véhicules -----
    void setupVehicules();
    void chargerVehiculesExemple();
    void refreshVehiculesTable();
    void refreshVehiculePropose();
    void filtrerVehicules();
    int vehiculeSelectionne();
    QString prochainIdVehicule();
    bool doitEchangerVehicules(Vehicule a, Vehicule b, int critere);
    StatCritere statVehiculesParEtat();
    StatCritere statVehiculesParDispo();
    QList<Vehicule> m_vehicules;
    int m_vehiculePropose; // numéro du véhicule proposé pour la mission (-1 = aucun)

    // ----- Outils communs -----
    QTableWidgetItem *creerItem(const QString &texte, const QColor &couleur, bool gras);
    void setLabelClass(QLabel *label, const QString &classe);
    void exporterTableauPdf(QTableWidget *table, const QString &titre, const QString &nomFichier);

    Ui::GSmartFireStation *ui;
};


