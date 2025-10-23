from flask import Flask, request, jsonify
import sqlite3
from datetime import datetime

app = Flask(__name__)

# ==========================================================
# INICIALIZAÇÃO DO BANCO DE DADOS
# ==========================================================
def init_db():
    conn = sqlite3.connect('leituras.db')
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS leituras (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            temperatura_c REAL,
            umidade_pct REAL,
            luminosidade INTEGER,
            presenca INTEGER,
            probabilidade_vida REAL
        )
    ''')
    conn.commit()
    conn.close()
    print("[DB] ✅ Banco de dados inicializado com sucesso.")

# ==========================================================
# ROTA DE RECEBIMENTO (POST)
# ==========================================================
@app.route('/leituras', methods=['POST'])
def receber_leituras():
    data = request.get_json(force=True)
    if not data:
        return jsonify({"erro": "JSON inválido"}), 400

    try:
        conn = sqlite3.connect('leituras.db')
        c = conn.cursor()
        c.execute('''
            INSERT INTO leituras (timestamp, temperatura_c, umidade_pct, luminosidade, presenca, probabilidade_vida)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (
            datetime.now().isoformat(timespec='seconds'),
            data.get('temperatura_c'),
            data.get('umidade_pct'),
            data.get('luminosidade'),
            data.get('presenca'),
            data.get('probabilidade_vida')
        ))
        conn.commit()
        conn.close()
        print(f"[API] 📡 Nova leitura recebida: {data}")
        return jsonify({"status": "sucesso"}), 201
    except Exception as e:
        print("[ERRO]", e)
        return jsonify({"erro": str(e)}), 500

# ==========================================================
# ROTA DE CONSULTA (GET)
# ==========================================================
@app.route('/leituras', methods=['GET'])
def listar_leituras():
    conn = sqlite3.connect('leituras.db')
    c = conn.cursor()
    c.execute('SELECT * FROM leituras ORDER BY id DESC LIMIT 100')
    dados = c.fetchall()
    conn.close()

    leituras = [
        {
            "id": row[0],
            "timestamp": row[1],
            "temperatura_c": row[2],
            "umidade_pct": row[3],
            "luminosidade": row[4],
            "presenca": bool(row[5]),
            "probabilidade_vida": row[6]
        } for row in dados
    ]
    return jsonify(leituras)

# ==========================================================
# EXECUÇÃO DO SERVIDOR
# ==========================================================
if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True)
