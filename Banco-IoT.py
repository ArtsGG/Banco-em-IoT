from flask import Flask, request, jsonify
from pymongo.mongo_client import MongoClient
from pymongo.server_api import ServerApi
from datetime import datetime

app = Flask(__name__)

# ==========================================================
# CONFIGURAÇÃO E INICIALIZAÇÃO DO MONGODB
# ==========================================================
uri = "mongodb+srv://arthursouza:Lu-300898@arthur.ui40ak0.mongodb.net/?appName=Arthur"
client = None
db = None

def init_db():
    global client, db
    try:
        # Create a new client and connect to the server
        client = MongoClient(uri, server_api=ServerApi('1'))
        
        # Send a ping to confirm a successful connection
        client.admin.command('ping')
        print("[DB] ✅ Conexão com MongoDB Atlas estabelecida com sucesso!")
        
        # Selecionar o banco de dados
        db = client['IoT']
        
    except Exception as e:
        print(f"[DB] ❌ Erro ao conectar ao MongoDB: {e}")
        raise e  # Re-raise the exception to prevent silent failures

# ==========================================================
# ROTA DE RECEBIMENTO (POST)
# ==========================================================
@app.route('/leituras', methods=['POST'])
def receber_leituras():
    data = request.get_json(force=True)
    if not data:
        return jsonify({"erro": "JSON inválido"}), 400

    try:
        # Adicionar timestamp ao documento
        data['timestamp'] = datetime.now().isoformat(timespec='seconds')
        
        # Inserir no MongoDB
        result = db.leituras.insert_one(data)
        
        print(f"[API] 📡 Nova leitura recebida: {data}")
        return jsonify({"status": "sucesso", "id": str(result.inserted_id)}), 201
    except Exception as e:
        print("[ERRO]", e)
        return jsonify({"erro": str(e)}), 500

# ==========================================================
# ROTA DE CONSULTA (GET)
# ==========================================================
@app.route('/leituras', methods=['GET'])
def listar_leituras():
    try:
        # Buscar as últimas 100 leituras
        leituras = list(db.leituras.find({}, {'_id': 0}).sort('timestamp', -1).limit(100))
        
        # Converter os valores de presença para boolean
        for leitura in leituras:
            if 'presenca' in leitura:
                leitura['presenca'] = bool(leitura['presenca'])
                
        return jsonify(leituras)
    except Exception as e:
        print("[ERRO]", e)
        return jsonify({"erro": str(e)}), 500

# ==========================================================
# EXECUÇÃO DO SERVIDOR
# ==========================================================
if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True)
