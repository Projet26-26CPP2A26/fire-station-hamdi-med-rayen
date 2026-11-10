SMART FIRE STATION — Qt + Oracle integration (Module Véhicules)
================================================

This project uses Qt SQL with the QODBC driver (Oracle ODBC driver "Oracle in XE").

1. Prepare the database (Oracle SQL Developer, F5 = Run Script)
   - Fresh database: run database.sql with the Oracle user that will own the
     tables. It creates PERSONNEL, VEHICULE, MISSION, MISSION_PERSONNEL,
     MISSION_VEHICULE, sequences, ID triggers, and sample rows.
   - Existing database (5 tables already created from the old schema): run
     mise_a_jour_personnel.sql ONCE instead. It adds the new Personnel columns
     (mot_de_passe, question_secrete, reponse_secrete) and constraints.
     Do NOT run both.

2. Open the Qt project
   - Open Vehicules.pro in Qt Creator (CMakeLists.txt is also provided).
   - Select your Qt kit (e.g. Desktop Qt 6.12.0 MinGW 64-bit).
   - Run qmake / Configure Project, then build and run.

3. First launch
   - The application asks for Oracle host, port, SID/service, username, password.
   - Defaults are localhost, 1521, and XE. Change them to match your Oracle setup.
   - The password is entered at runtime and is not saved in the project.
   - If the connection is cancelled or fails, the app opens with demo data.
     Changes made in demo mode are not saved to Oracle.

4. QODBC prerequisites
   - Qt SQL module and the QODBC driver plugin (included with Qt).
   - Oracle ODBC driver "Oracle in XE" installed (Oracle Instant Client + ODBC
     package), same architecture as your Qt kit (64-bit).
   - If the connection fails, read the error message, verify the service name
     and that the Oracle listener is running.

Database mapping notes
----------------------
- Oracle numeric IDs are shown as V001/V002... in the UI.
- UI labels are mapped to schema values: Citerne <-> Camion-citerne and
  Bon <-> Operationnel.
- The BEFORE INSERT triggers fill the IDs automatically (do not remove them:
  the Qt INSERT statements do not provide id_vehicule).
- Personnel test accounts (module Personnel): ahmed@station.tn / sami@station.tn,
  password Admin123, secret answer "tunis" (stored as SHA-256 hashes).