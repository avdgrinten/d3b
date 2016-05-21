
const common = require('./common');
const d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testInsert: common.defaultTest((test, client) => {
	test.expect(1000);

	let data = [ ];
	for(let i = 0; i < 1000; i++) {
		data.push(Buffer.from('item #' + i));
	}

	let written = [ ];

	return d3bUtil.createStorage(client, {
		driver: 'FlexStorage',
		identifier: 'test-storage'
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
		return Promise.all(written.map(entry => {
			return d3bUtil.fetch(client, {
				storageName: 'test-storage',
				documentId: entry.documentId
			})
			.then(result => {
				test.ok(entry.source.equals(result));
			});
		}));
	});
})

};

