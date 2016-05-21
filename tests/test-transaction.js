
const common = require('./common');
const d3bUtil = require('../client-nodejs/d3b-util');

module.exports = {

testCommit: common.defaultTest((test, client) => {
	test.expect(1);

	let source = Buffer.from('Hello world!');

	return d3bUtil.createStorage(client, {
		driver: 'FlexStorage',
		identifier: 'test-storage'
	})
	.then(() => {
		return d3bUtil.transaction(client, { });
	})
	.then(transaction_id => {
		return d3bUtil.update(client, {
			transactionId: transaction_id,
			mutations: [ {
				type: d3bUtil.kMutateInsert,
				storageName: 'test-storage',
				buffer: source
			} ]
		})
		.then(() => {
			return d3bUtil.apply(client, {
				transactionId: transaction_id,
				type: d3bUtil.kApplySubmit
			});
		})
		.then(() => {
			return d3bUtil.apply(client, {
				transactionId: transaction_id,
				type: d3bUtil.kApplyCommit
			});
		});
	})
	.then(() => {
		return d3bUtil.fetch(client, {
			storageName: 'test-storage',
			documentId: 1
		})
		.then(buffer => {
			test.ok(source.equals(buffer));
		});
	});
}),

testRollback: common.defaultTest((test, client) => {
	test.expect(0);

	let source = Buffer.from('Hello world!');

	return d3bUtil.createStorage(client, {
		driver: 'FlexStorage',
		identifier: 'test-storage'
	})
	.then(() => {
		return d3bUtil.transaction(client, { });
	})
	.then(transaction_id => {
		return d3bUtil.update(client, {
			transactionId: transaction_id,
			mutations: [ {
				type: d3bUtil.kMutateInsert,
				storageName: 'test-storage',
				buffer: source
			} ]
		})
		.then(() => {
			return d3bUtil.apply(client, {
				transactionId: transaction_id,
				type: d3bUtil.kApplySubmit
			});
		})
		.then(() => {
			return d3bUtil.apply(client, {
				transactionId: transaction_id,
				type: d3bUtil.kApplyRollback
			});
		});
	})
	.then(() => {
		console.log("FIXME: verify that the document was NOT written!");
	});
})

};

