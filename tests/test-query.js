
const common = require('./common');
const d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testQuery: common.defaultTest((test, client) => {
	test.expect(1000);

	let data = [ ];
	for(let i = 0; i < 1000; i++) {
		data.push(Buffer.from('item #' + i));
	}

	let file = require('fs').readFileSync('tests/views/simple-view.js');

	let written = [ ];

	return d3bUtil.uploadExtern(client, {
		fileName: 'simple-view.js',
		buffer: file
	})
	.then(() => {
		return d3bUtil.createStorage(client, {
			driver: 'FlexStorage',
			identifier: 'test-storage'
		});
	})
	.then(() => {
		return d3bUtil.createView(client, {
			driver: 'JsView',
			identifier: 'test-view',
			baseStorage: 'test-storage',
			scriptFile: 'simple-view.js'
		});
	})
	.then(() => {
		return Promise.all(data.map(buffer => {
			return d3bUtil.insert(client, {
				storageName: 'test-storage',
				buffer: buffer
			}).then(result => {
				written.push({
					source: buffer,
					documentId: result.documentId
				});
			});
		}));
	})
	.then(() => {
		return d3bUtil.query(client, {
			viewName: 'test-view'
		}, data => {
			let json = JSON.parse(data.toString());
			let entry = written.find(entry => entry.documentId == json.id);
			test.equals(entry.source.toString(), json.buffer);
		});
	});
/*			var queue = [ ];
			d3bUtil.query(client, { viewName: 'test-view' },
				function(row) {
					queue.push(JSON.parse(row).id);
				}, function() {
					async.each(queue, function(id, callback) {
						d3bUtil.modify(client, { storageName: 'test-storage',
								id: id, buffer: 'modified-' + id }, function() {
							callback();
						});
					}, callback);
				});
		},
		function(callback) {
			var retrieved = [ ];
			d3bUtil.query(client, { viewName: 'test-view' },
				function(row) {
					retrieved.push(JSON.parse(row));
				}, function() {
					test.equal(retrieved.length, data.length);
					for(var i = 0; i < data.length; i++)
						test.equal(retrieved[i].buffer, 'modified-' + retrieved[i].id);
					callback();
				});
		},
	});*/
}),

};

